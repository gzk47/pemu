//
// gamelist 编码处理：GBK -> UTF-8（内置映射表，不依赖 iconv）
//

#include <cstdio>
#include <cstdint>
#include <vector>

#include "encoding.h"
#include "gbk_table.h"

namespace {

    // 分块大小：转换时峰值内存约为 32K(读) + 96K(写)
    const size_t kChunk = 32 * 1024;
    const uint16_t kReplacement = 0xFFFD;

    // GBK 双字节 -> Unicode；0 表示无映射
    inline uint16_t gbkLookup(unsigned char lead, unsigned char trail) {
        int index = ((int) lead - 0x81) * 190
                    + ((trail < 0x7F) ? (int) trail - 0x40 : (int) trail - 0x41);
        return kGbkTable[index];
    }

    inline bool isGbkTrail(unsigned char c) {
        return (c >= 0x40 && c <= 0x7E) || (c >= 0x80 && c <= 0xFE);
    }

    inline size_t putUtf8(uint16_t u, unsigned char *out) {
        if (u == 0) {
            u = kReplacement;
        }
        if (u < 0x80) {
            out[0] = (unsigned char) u;
            return 1;
        }
        if (u < 0x800) {
            out[0] = (unsigned char) (0xC0 | (u >> 6));
            out[1] = (unsigned char) (0x80 | (u & 0x3F));
            return 2;
        }
        out[0] = (unsigned char) (0xE0 | (u >> 12));
        out[1] = (unsigned char) (0x80 | ((u >> 6) & 0x3F));
        out[2] = (unsigned char) (0x80 | (u & 0x3F));
        return 3;
    }

    // UTF-8 流式校验状态机（拒绝 overlong / 代理区 / 超范围 / 截断序列）
    struct Utf8Checker {
        int need = 0;
        unsigned char lo = 0x80;
        unsigned char hi = 0xBF;

        void start(int n, unsigned char l, unsigned char h) {
            need = n;
            lo = l;
            hi = h;
        }

        bool feed(unsigned char c) {
            if (need > 0) {
                if (c < lo || c > hi) {
                    return false;
                }
                need--;
                lo = 0x80;
                hi = 0xBF;
                return true;
            }
            if (c < 0x80) return true;
            if (c >= 0xC2 && c <= 0xDF) { start(1, 0x80, 0xBF); return true; }
            if (c == 0xE0) { start(2, 0xA0, 0xBF); return true; }
            if (c >= 0xE1 && c <= 0xEC) { start(2, 0x80, 0xBF); return true; }
            if (c == 0xED) { start(2, 0x80, 0x9F); return true; }
            if (c >= 0xEE && c <= 0xEF) { start(2, 0x80, 0xBF); return true; }
            if (c == 0xF0) { start(3, 0x90, 0xBF); return true; }
            if (c >= 0xF1 && c <= 0xF3) { start(3, 0x80, 0xBF); return true; }
            if (c == 0xF4) { start(3, 0x80, 0x8F); return true; }
            return false;
        }

        bool complete() const { return need == 0; }
    };

    bool convertFile(const std::string &srcPath, const std::string &dstPath) {
        FILE *in = fopen(srcPath.c_str(), "rb");
        if (!in) {
            return false;
        }
        FILE *out = fopen(dstPath.c_str(), "wb");
        if (!out) {
            fclose(in);
            return false;
        }

        std::vector<unsigned char> inBuf(kChunk);
        // 单个非法字节最多输出 3 字节 U+FFFD；跨块 pending 失败时还会多出 3 字节
        std::vector<unsigned char> outBuf(kChunk * 3 + 3);
        // 跨块保留的 GBK 首字节（-1 表示无）
        int pending = -1;
        bool ok = true;
        size_t n;

        while ((n = fread(inBuf.data(), 1, kChunk, in)) > 0) {
            size_t outLen = 0;
            size_t i = 0;

            if (pending >= 0 && n > 0) {
                unsigned char trail = inBuf[0];
                if (isGbkTrail(trail)) {
                    outLen += putUtf8(gbkLookup((unsigned char) pending, trail), &outBuf[outLen]);
                    i = 1;
                } else {
                    // 首字节后不是合法尾字节，丢弃首字节
                    outLen += putUtf8(0, &outBuf[outLen]);
                }
                pending = -1;
            }

            for (; i < n; i++) {
                unsigned char c = inBuf[i];
                if (c < 0x80) {
                    outBuf[outLen++] = c;
                    continue;
                }
                if (c >= 0x81 && c <= 0xFE) {
                    if (i + 1 >= n) {
                        pending = c;
                        break;
                    }
                    unsigned char trail = inBuf[i + 1];
                    if (isGbkTrail(trail)) {
                        outLen += putUtf8(gbkLookup(c, trail), &outBuf[outLen]);
                        i++;
                    } else {
                        outLen += putUtf8(0, &outBuf[outLen]);
                    }
                    continue;
                }
                // 0x80 / 0xFF 在 GBK 中非法
                outLen += putUtf8(0, &outBuf[outLen]);
            }

            if (fwrite(outBuf.data(), 1, outLen, out) != outLen) {
                ok = false;
                break;
            }
        }

        if (ok && pending >= 0) {
            unsigned char tail[3];
            size_t len = putUtf8(0, tail);
            if (fwrite(tail, 1, len, out) != len) {
                ok = false;
            }
        }
        if (ferror(in)) {
            ok = false;
        }

        fclose(in);
        if (fclose(out) != 0) {
            ok = false;
        }
        if (!ok) {
            ::remove(dstPath.c_str());
        }
        return ok;
    }
}

namespace pemu {
    namespace encoding {

        static_assert(sizeof(kGbkTable) / sizeof(kGbkTable[0]) == 23940,
                      "kGbkTable size mismatch");

        bool isUtf8File(const std::string &path) {
            FILE *f = fopen(path.c_str(), "rb");
            if (!f) {
                // 打不开时不做判断，交给原有逻辑处理
                return true;
            }

            unsigned char buf[8192];
            Utf8Checker checker;
            bool ok = true;
            size_t n;

            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                for (size_t i = 0; i < n; i++) {
                    if (!checker.feed(buf[i])) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) {
                    break;
                }
            }
            fclose(f);

            return ok && checker.complete();
        }

        std::string createUtf8CopyIfNeeded(const std::string &path, const std::string &tmpPath) {
            if (isUtf8File(path)) {
                return "";
            }
            if (!convertFile(path, tmpPath)) {
                printf("encoding: gbk to utf8 conversion failed for %s\n", path.c_str());
                return "";
            }
            printf("encoding: gbk gamelist converted: %s -> %s\n", path.c_str(), tmpPath.c_str());
            return tmpPath;
        }
    }
}

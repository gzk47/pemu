//
// sort_key：ROM 显示名的排序键（数字 / 英文 / 中文拼音首字母三段式）
//

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include "sort_key.h"
#include "pinyin_initials.h"

namespace {

    // 解码一个 UTF-8 码点，返回消耗的字节数；非法序列按单字节处理（回退组，不崩溃）
    size_t decodeUtf8(const std::string &s, size_t pos, uint32_t &cp) {
        const unsigned char b = static_cast<unsigned char>(s[pos]);
        if (b < 0x80) {
            cp = b;
            return 1;
        }
        size_t len;
        uint32_t v;
        if ((b & 0xE0) == 0xC0) {
            len = 2;
            v = b & 0x1F;
        } else if ((b & 0xF0) == 0xE0) {
            len = 3;
            v = b & 0x0F;
        } else if ((b & 0xF8) == 0xF0) {
            len = 4;
            v = b & 0x07;
        } else {
            cp = b;
            return 1;
        }
        if (pos + len > s.size()) {
            cp = b;
            return 1;
        }
        for (size_t i = 1; i < len; i++) {
            const unsigned char c = static_cast<unsigned char>(s[pos + i]);
            if ((c & 0xC0) != 0x80) {
                cp = b;
                return 1;
            }
            v = (v << 6) | (c & 0x3F);
        }
        cp = v;
        return len;
    }

    // 归组前要跳过的符号：ASCII 非字母数字，以及常见的全角/中文标点
    bool isSkippedPrefix(uint32_t cp) {
        if (cp < 0x80) {
            return !((cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'));
        }
        return cp == 0x3000                       // 全角空格
               || (cp >= 0x3001 && cp <= 0x3003)  // 、。〃
               || (cp >= 0x3008 && cp <= 0x3011)  // 〈〉《》「」『』【】
               || (cp >= 0x3014 && cp <= 0x301F)  // 〔〕〖〗等括号/引号
               || cp == 0x3030                    // 〰
               || (cp >= 0x2010 && cp <= 0x2027)  // 连字符、破折号、引号、省略号
               || (cp >= 0xFF01 && cp <= 0xFF0F)  // ！＂＃…／
               || (cp >= 0xFF1A && cp <= 0xFF20)  // ：；＜＝＞？＠
               || (cp >= 0xFF3B && cp <= 0xFF40)  // ［＼］＾＿｀
               || (cp >= 0xFF5B && cp <= 0xFF65)  // ｛｜｝～等
               || (cp >= 0xFE10 && cp <= 0xFE19); // 竖排标点
    }

    bool isAsciiLetter(uint32_t cp) {
        return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
    }

    char lowerAscii(unsigned char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
    }

    // 查拼音首字母；表外返回 0（调用方回退到码位序）
    char pinyinInitial(uint32_t cp) {
        const auto &cps = pemu::pinyin_table::kCodePoints;
        const auto &initials = pemu::pinyin_table::kInitials;
        size_t lo = 0, hi = pemu::pinyin_table::kCount;
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            if (cps[mid] == cp) {
                return initials[mid];
            }
            if (cps[mid] < cp) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return 0;
    }
}

std::string pemu::makeSortKey(const std::string &name) {
    // 找第一个有效字符（跳过前导符号），确定归组
    size_t pos = 0;
    size_t start = 0;
    uint32_t cp = 0;
    bool found = false;
    while (pos < name.size()) {
        const size_t len = decodeUtf8(name, pos, cp);
        if (!isSkippedPrefix(cp)) {
            found = true;
            start = pos;
            break;
        }
        pos += len;
    }
    if (!found) {
        return std::string("3") + name; // 全为符号/空名：统一放最后，保持原字节序
    }

    // 数字组：开头连续数字按数值比较（零填充 10 位），再拼剩余部分
    if (cp >= '0' && cp <= '9') {
        std::string digits;
        size_t p = start;
        while (p < name.size() && digits.size() < 10) {
            uint32_t c = 0;
            const size_t len = decodeUtf8(name, p, c);
            if (c < '0' || c > '9') {
                break;
            }
            digits.push_back(static_cast<char>(c));
            p += len;
        }
        uint64_t value = 0;
        for (const char d: digits) {
            value = value * 10 + static_cast<uint64_t>(d - '0');
        }
        char buf[24];
        snprintf(buf, sizeof(buf), "%010llu", static_cast<unsigned long long>(value));
        std::string key = std::string("0") + buf;
        // 剩余部分（超过 10 位的数字并入这里）：ASCII 小写
        for (size_t i = p; i < name.size(); i++) {
            key.push_back(lowerAscii(static_cast<unsigned char>(name[i])));
        }
        return key;
    }

    // 英文组：整名小写，与原 strcasecmp / tolower 比较结果一致
    if (isAsciiLetter(cp)) {
        std::string key = "1";
        for (size_t i = 0; i < name.size(); i++) {
            key.push_back(lowerAscii(static_cast<unsigned char>(name[i])));
        }
        return key;
    }

    // 中文组：拼音首字母 + 原 UTF-8 名（同首字母内即按码位序）
    const char initial = pinyinInitial(cp);
    if (initial >= 'a' && initial <= 'z') {
        return std::string("2") + initial + name;
    }

    // 表外字符（生僻字、假名、韩文、非法字节）：中文组末尾，组内按原字节序
    return std::string("2{") + name;
}

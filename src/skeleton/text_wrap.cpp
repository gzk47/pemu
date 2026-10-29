//
// pEMU 渲染层中文断行：
// 复用 c2d::Text 的度量（findCharacterPos），只把「可断点」从空白字符
// 扩展为「空白字符或 CJK 字符边界」，输出以 '\n' 连接的多行字符串；
// 行高/行距/字距与首行、换行渲染仍完全由控件负责。
//

#include "pemu.h"
#include "text_wrap.h"

#include <vector>

using namespace c2d;

namespace pemu {

namespace {

// CJK 可断码点：汉字/假名/谚文/兼容表意/全角形式，以及中文常用的破折号与省略号
bool isCjkCodepoint(uint32_t cp) {
    return (cp >= 0x2E80 && cp <= 0x9FFF)    // 部首/中日韩符号/假名/注音/统一汉字
           || (cp >= 0xAC00 && cp <= 0xD7AF) // 谚文音节
           || (cp >= 0xF900 && cp <= 0xFAFF) // CJK 兼容表意文字
           || (cp >= 0xFF00 && cp <= 0xFFEF) // 全角形式（，！？：；（）等）
           || cp == 0x2013 || cp == 0x2014   // – —
           || cp == 0x2026;                  // …
}

// 行首禁排（闭合标点）：不能出现在行首，token 化时吸附到前一个字符
bool isNoLineStartCp(uint32_t cp) {
    switch (cp) {
        case 0xFF0C: // ，
        case 0x3002: // 。
        case 0x3001: // 、
        case 0xFF1B: // ；
        case 0xFF1A: // ：
        case 0xFF01: // ！
        case 0xFF1F: // ？
        case 0xFF09: // ）
        case 0x300D: // 」
        case 0x300F: // 』
        case 0x300B: // 》
        case 0x3009: // 〉
        case 0x3015: // 〕
        case 0xFF3D: // ］
        case 0xFF5D: // ｝
        case 0xFF05: // ％
        case 0x00B0: // °
        case 0x2103: // ℃
        case 0x2026: // …
        case 0x2014: // —
        case 0x00B7: // ·
        case ',':
        case '.':
        case ';':
        case ':':
        case '!':
        case '?':
        case ')':
        case ']':
        case '}':
        case '%':
            return true;
        default:
            return false;
    }
}

// 行尾禁排（开括号类）：不能出现在行尾，token 化时吸附到后一个字符
bool isNoLineEndCp(uint32_t cp) {
    switch (cp) {
        case 0xFF08: // （
        case 0x300C: // 「
        case 0x300E: // 『
        case 0x300A: // 《
        case 0x3008: // 〈
        case 0x3014: // 〔
        case 0xFF3B: // ［
        case 0xFF5B: // ｛
        case '(':
        case '[':
        case '{':
            return true;
        default:
            return false;
    }
}

// 解码一个 UTF-8 码点；非法序列按 U+FFFD 处理并只前进 1 字节（绝不在码点中间切割）
uint32_t decodeCodepoint(const std::string &s, size_t &i) {
    unsigned char c = (unsigned char) s[i];
    if (c < 0x80) {
        i += 1;
        return c;
    }
    int len = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) {
        len = 2;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        len = 3;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        len = 4;
        cp = c & 0x07;
    }
    if (len == 0 || i + (size_t) len > s.size()) {
        i += 1;
        return 0xFFFD;
    }
    for (int k = 1; k < len; k++) {
        unsigned char cc = (unsigned char) s[i + (size_t) k];
        if ((cc & 0xC0) != 0x80) {
            i += 1;
            return 0xFFFD;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += (size_t) len;
    return cp;
}

// U+3000（全角空格）
bool isIdeographicSpaceAt(const std::string &s, size_t i) {
    return i + 2 < s.size()
           && (unsigned char) s[i] == 0xE3
           && (unsigned char) s[i + 1] == 0x80
           && (unsigned char) s[i + 2] == 0x80;
}

bool isSpaceByte(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

// 从 i 起构建一个不可再分单元：连续非空白、非 CJK 码点（英文单词整体），
// 或单个 CJK 码点；返回单元结束位置（一定在码点边界上）
size_t buildOneUnit(const std::string &s, size_t &i) {
    uint32_t cp = decodeCodepoint(s, i);
    if (!isCjkCodepoint(cp)) {
        while (i < s.size()) {
            if (isSpaceByte(s[i]) || s[i] == '\n' || isIdeographicSpaceAt(s, i)) {
                break;
            }
            size_t j = i;
            uint32_t next = decodeCodepoint(s, j);
            if (isCjkCodepoint(next)) {
                break;
            }
            i = j;
        }
    }
    return i;
}

struct WrapToken {
    size_t start;   // 源字符串字节起点（码点边界）
    size_t end;     // 源字符串字节终点（码点边界）
    bool newline;   // 显式 '\n' 强制断行
    WrapToken(size_t s, size_t e, bool nl) : start(s), end(e), newline(nl) {}
};

} // namespace

bool hasCjkText(const std::string &src) {
    for (size_t i = 0; i < src.size();) {
        if (isCjkCodepoint(decodeCodepoint(src, i))) {
            return true;
        }
    }
    return false;
}

std::string wrapText(Font *font, unsigned int charSize, const std::string &src, float maxWidth) {
    if (font == nullptr || maxWidth <= 0 || src.empty()) {
        return src;
    }

    // 快路径：不含 CJK 的文本原样返回——英文排版完全交给控件原生按词折行，零变化
    if (!hasCjkText(src)) {
        return src;
    }

    // 测量用临时 Text：与显示控件同字体同字号，宽度一律用 findCharacterPos 首尾差值
    // （与 ui_listbox 截断、控件排版同源度量）；不设 sizeMax，避免其自身折行干扰测量
    Text measure("", charSize, font);
    auto lineWidth = [&measure](const std::string &s) -> float {
        if (s.empty()) {
            return 0.0f;
        }
        measure.setString(s);
        size_t count = 0;
        for (size_t i = 0; i < s.size();) {
            decodeCodepoint(s, i);
            count++;
        }
        return measure.findCharacterPos(count).x - measure.findCharacterPos(0).x;
    };

    // token 化：英文/数字/符号连续串为一个不可拆 token；CJK 码点单独成 token；
    // 行首禁排标点吸附到前一个 token；行尾禁排标点吸附到后一个 token
    std::vector<WrapToken> tokens;
    size_t i = 0;
    while (i < src.size()) {
        if (src[i] == '\n') {
            tokens.push_back(WrapToken(i, i + 1, true));
            i += 1;
            continue;
        }
        if (isIdeographicSpaceAt(src, i)) {
            i += 3;
            continue;
        }
        if (isSpaceByte(src[i])) {
            i += 1;
            continue;
        }
        size_t start = i;
        size_t end = buildOneUnit(src, i);
        // 行尾禁排：token 以开括号结尾时，把与之紧连的下一个单元并入
        while (true) {
            size_t k = end - 1;
            while (k > start && ((unsigned char) src[k] & 0xC0) == 0x80) {
                k--;
            }
            size_t j = k;
            uint32_t last = decodeCodepoint(src, j);
            if (!isNoLineEndCp(last)) {
                break;
            }
            if (i >= src.size() || isSpaceByte(src[i]) || src[i] == '\n'
                || isIdeographicSpaceAt(src, i)) {
                break;
            }
            size_t newEnd = buildOneUnit(src, i);
            if (newEnd <= end) {
                break; // 防御：无进展则退出
            }
            end = newEnd;
        }
        // 行首禁排：与之紧连的闭合标点并入当前 token（可连续多个）
        while (i < src.size() && !isSpaceByte(src[i]) && src[i] != '\n'
               && !isIdeographicSpaceAt(src, i)) {
            size_t j = i;
            uint32_t next = decodeCodepoint(src, j);
            if (!isNoLineStartCp(next)) {
                break;
            }
            i = j;
            end = i;
        }
        tokens.push_back(WrapToken(start, end, false));
    }

    // 贪心装行：判据与控件一致（候选行按 findCharacterPos 度量 ≤ maxWidth）；
    // 换行处插入 '\n'，行首不带空白，行中 token 之间的空白按源文本拼回
    std::vector<std::string> lines;
    std::string cur;
    bool curEmpty = true;
    size_t prevEnd = 0;

    auto flushLine = [&]() {
        lines.push_back(cur);
        cur.clear();
        curEmpty = true;
    };

    for (const auto &t: tokens) {
        if (t.newline) {
            flushLine();
            continue;
        }
        std::string token = src.substr(t.start, t.end - t.start);
        bool placed = false;
        if (!curEmpty) {
            std::string gap = (t.start > prevEnd) ? src.substr(prevEnd, t.start - prevEnd) : "";
            std::string candidate = cur + gap + token;
            if (lineWidth(candidate) <= maxWidth) {
                cur = candidate;
                placed = true;
            } else {
                flushLine();
            }
        }
        if (!placed) {
            if (lineWidth(token) > maxWidth) {
                // 单 token 超宽（超长英文词/URL 等，纯 ASCII）：按码点硬切，不做禁则调整
                size_t pos = t.start;
                while (pos < t.end) {
                    std::string piece;
                    size_t k = pos;
                    while (k < t.end) {
                        size_t j = k;
                        decodeCodepoint(src, j);
                        std::string trial = piece + src.substr(k, j - k);
                        if (lineWidth(trial) > maxWidth) {
                            break;
                        }
                        piece = trial;
                        k = j;
                    }
                    if (k == pos) {
                        // 单码点即超宽的极端情况：强制放一个码点，保证前进
                        size_t j = pos;
                        decodeCodepoint(src, j);
                        piece = src.substr(pos, j - pos);
                        k = j;
                    }
                    pos = k;
                    if (pos < t.end) {
                        lines.push_back(piece);
                    } else {
                        cur = piece;
                        curEmpty = false;
                    }
                }
            } else {
                cur = token;
                curEmpty = false;
            }
        }
        prevEnd = t.end;
    }
    flushLine();

    std::string out;
    for (size_t k = 0; k < lines.size(); k++) {
        if (k > 0) {
            out += '\n';
        }
        out += lines[k];
    }
    return out;
}

} // namespace pemu

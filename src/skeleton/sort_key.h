//
// sort_key：ROM 显示名的排序键（数字 / 英文 / 中文拼音首字母三段式）
//

#ifndef C2DUI_SORT_KEY_H
#define C2DUI_SORT_KEY_H

#include <string>

namespace pemu {

    /**
     * 生成可直接按字节序比较的排序键，规则：
     *  - 组间：数字 "0" < 英文 "1" < 中文 "2" < 无有效字符 "3"
     *  - 数字组：开头连续数字按数值零填充 10 位（"2" -> "0000000002"，"10" -> "0000000010"），
     *            再拼剩余部分（ASCII 转小写）
     *  - 英文组：整名转小写（与历史 strcasecmp 行为一致）
     *  - 中文组：拼音首字母 + 原 UTF-8 名称；拼音表外字符（生僻字/假名/韩文等）
     *            以 '2{' 开头，排在中文组末尾，组内按原字节（码位）序
     *  - 名称开头的引号、括号、连字符、空格等符号先跳过，以第一个有效字符归组
     *
     * 纯函数，无全局可变状态；同一输入恒返回同一输出。
     */
    std::string makeSortKey(const std::string &name);
}

#endif //C2DUI_SORT_KEY_H

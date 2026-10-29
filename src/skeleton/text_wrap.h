//
// pEMU 渲染层中文断行
//

#ifndef PEMU_TEXT_WRAP_H
#define PEMU_TEXT_WRAP_H

#include "pemu.h"

namespace pemu {

// 不能让控件再按空格分词折行（详见 ui_romlist_rominfo.cpp 中的说明）。
bool hasCjkText(const std::string &src);

// 按 UTF-8 码点边界折行（复用 c2d::Text 的度量与排版语义）：
// - 不含 CJK 的文本原样返回，英文排版完全交给控件原生按词折行，零变化
// - CJK 逐字可断；英文/数字/符号连续串视为整体单词不拆
// - 行首避开 ，。、；：！？）」』等闭合标点（连同前一个字符一起下移）
// - 宽度测量与控件一致（findCharacterPos 首尾差值），只决定 '\n' 插入位置；
//   行高/行距/字距与换行渲染仍完全由控件负责
// - 返回以 '\n' 连接的多行字符串；调用方在文本变化时调用一次即可，不要每帧调用
std::string wrapText(c2d::Font *font, unsigned int charSize, const std::string &src, float maxWidth);

}

#endif //PEMU_TEXT_WRAP_H

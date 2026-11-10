//
// Created by cpasjuste on 05/01/18.
//

#include "pemu.h"

using namespace c2d;
using namespace pemu;
using namespace ss_api;

// 跑马灯辅助：只在 UTF-8 码点边界上计数与切片（不改动原有的截断/显示逻辑）
static size_t utf8CharCount(const std::string &str) {
    size_t count = 0;
    for (size_t i = 0; i < str.size(); i++) {
        if (((unsigned char) str[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static std::string utf8Sub(const std::string &str, size_t startChar, size_t chars) {
    size_t count = 0;
    size_t begin = str.size();
    size_t end = str.size();

    for (size_t i = 0; i < str.size(); i++) {
        if (((unsigned char) str[i] & 0xC0) != 0x80) {
            if (count == startChar && begin >= str.size()) {
                begin = i;
            }
            if (count == startChar + chars) {
                end = i;
                break;
            }
            count++;
        }
    }

    if (begin >= str.size()) {
        return "";
    }
    return str.substr(begin, end - begin);
}

// 跑马灯速度：按像素换算（30~40 px/s 是舒适阅读区间），
// 每个字的停留时间 = 该字实际像素宽度 / 速度，汉字与英文单字自动区别对待
// 注：不做 setPosition 像素平移。cross2d 的 Text 没有「屏幕空间固定矩形裁剪」：
// Text::onDraw 直接画顶点，唯一相关逻辑是 ensureGeometryUpdate() 里
// "x > m_max_size.x 就 break"——它相对 Text 自身原点、且整字丢弃、还随平移一起移动，
// 无法当固定视口用；平移会让字溢出到左侧图标或右侧预览区。
static const float kMarqueeSpeedPxPerSec = 35.0f;
static const int kMarqueeStartDelayMs = 2000; // 开头（含回卷到起点）先停留，让用户先读到前几个字
static const int kMarqueeTailHoldMs = 2000;   // 滚到尾部后的停顿（不短于开头停留）
static const int kMarqueeMinStepMs = 30;      // 防止零宽字符原地打转
static const int kMarqueeMaxStepMs = 1000;    // 单个字的停留上限（异常宽字符保护）

UIListBoxLine::UIListBoxLine(
        const FloatRect &rect, const std::string &str,
        Font *font, unsigned int fontSize, const c2d::Vector2f &scaling, Texture *i, bool use_ic) : RectangleShape(
        rect) {
    //printf("ListBoxLine(%p)\n", this);
    icon = i;
    use_icons = use_ic;
    text = new Text(str, fontSize, font);
    text->setOutlineColor(Color::Black);
    text->setOrigin(Origin::Left);

    UIListBoxLine::setFillColor(Color::Transparent);

    if (use_icons) {
        // add iconRect
        iconRect = new RectangleShape(Vector2f(32, 32));
        iconRect->setPosition(Vector2f(1, 1));
        iconRect->setFillColor(Color::Transparent);
        iconRect->setOutlineThickness(1);
        UIListBoxLine::add(iconRect);
        // icon added in ListBox::setSelection (setIcon)
        // set text
        text->setPosition(iconRect->getSize().x + 8, UIListBoxLine::getSize().y / 2);
        marqueeWidth = UIListBoxLine::getSize().x - (float) fontSize - iconRect->getSize().x;
        text->setSizeMax(UIListBoxLine::getSize().x - (float) fontSize - iconRect->getSize().x, 0);
    } else {
        text->setPosition(2 * scaling.x, UIListBoxLine::getSize().y / 2);
        marqueeWidth = UIListBoxLine::getSize().x - text->getSize().x - (2 * scaling.x) - 2;
        text->setSizeMax(UIListBoxLine::getSize().x - text->getSize().x - (2 * scaling.x) - 2, 0);
    }

    UIListBoxLine::add(text);
}

void UIListBoxLine::setSize(const Vector2f &size) {
    UIListBoxLine::setSize(size.x, size.y);
}

void UIListBoxLine::setSize(float width, float height) {
    RectangleShape::setSize(width, height);
    marqueeWidth = getSize().x - 8;
    text->setSizeMax(getSize().x - 8, 0);
}

void UIListBoxLine::setString(const std::string &string) {
    // 原实现保持不变（text->setString(string) 仍是本函数唯一设置文本的语句）：
    // 只在前面记录完整名称并重置跑马灯状态；非高亮行显示结果与改动前完全一致
    fullString = string;
    charOffsets.clear();
    marqueeStart = 0;
    marqueeHoldTicks = 0;
    marqueeClock.restart();
    text->setString(string);
}

void UIListBoxLine::setMarqueeEnabled(bool enable) {
    if (marqueeEnabled == enable) {
        return;
    }

    marqueeEnabled = enable;
    charOffsets.clear();
    marqueeStart = 0;
    marqueeHoldTicks = 0;
    marqueeClock.restart();
    if (!enable) {
        // 关闭时恢复为改动前的显示（原实现就是直接显示完整字符串）
        text->setString(fullString);
    }
}

void UIListBoxLine::updateMarquee() {
    if (!marqueeEnabled || !isVisible() || marqueeWidth <= 0 || fullString.empty()) {
        return;
    }

    size_t count = utf8CharCount(fullString);
    if (count < 2) {
        return;
    }

    // 码点左边界表：只在字符串或宽度变化后计算一次，避免每帧重跑测量
    if (charOffsets.size() != count + 1) {
        text->setString(fullString);
        float base = text->findCharacterPos(0).x;
        charOffsets.resize(count + 1);
        for (size_t i = 0; i <= count; i++) {
            charOffsets[i] = text->findCharacterPos(i).x - base;
        }
        marqueeStart = 0;
        marqueeHoldTicks = 0;
        marqueeClock.restart();
    }

    if (charOffsets[count] <= marqueeWidth) {
        // 整名放得下：不需要滚动，保持完整显示（与改动前一致）
        text->setString(fullString);
        return;
    }

    // 从当前起点最多能放下的码点数（窗口宽度受 marqueeWidth 限制）
    size_t visible = 0;
    while (marqueeStart + visible < count &&
           charOffsets[marqueeStart + visible + 1] - charOffsets[marqueeStart] <= marqueeWidth) {
        visible++;
    }
    if (visible == 0) {
        visible = 1;
    }

    // 本拍应停留多久：起点先停（便于读到开头几个字）；滚到尾部再停；
    // 其余按「即将移出窗口的那个字」的实际像素宽度 / 速度 换算，不用固定每字 X 毫秒
    int dwellMs;
    if (marqueeStart + visible >= count) {
        dwellMs = kMarqueeTailHoldMs;
    } else if (marqueeStart == 0) {
        dwellMs = kMarqueeStartDelayMs;
    } else {
        float charWidth = charOffsets[marqueeStart + 1] - charOffsets[marqueeStart];
        dwellMs = (int) (charWidth / kMarqueeSpeedPxPerSec * 1000.0f);
        if (dwellMs < kMarqueeMinStepMs) {
            dwellMs = kMarqueeMinStepMs;
        }
        if (dwellMs > kMarqueeMaxStepMs) {
            dwellMs = kMarqueeMaxStepMs;
        }
    }
    marqueeHoldTicks = dwellMs; // 复用该成员记录当前停留时长（毫秒），避免改动 ui_listbox.h

    if (marqueeClock.getElapsedTime().asMilliseconds() >= dwellMs) {
        marqueeClock.restart();
        if (marqueeStart + visible >= count) {
            // 尾部停够：回到开头（下一拍按开头停顿再停留一次）
            marqueeStart = 0;
        } else {
            marqueeStart++;
        }
    }

    text->setString(utf8Sub(fullString, marqueeStart, visible));
}

void UIListBoxLine::setColor(const Color &color) {
    text->setFillColor(color);
    if (iconRect != nullptr) {
        iconRect->setOutlineColor(color);
    }
    if (icon != nullptr) {
        icon->setOutlineColor(color);
    }
}

void UIListBoxLine::setIcon(Texture *i) {
    if (use_icons) {
        if (icon != nullptr) {
            remove(icon);
        }
        icon = i;
        if (icon != nullptr) {
            if (icon->available) {
                icon->setOutlineThickness(1);
                icon->setOrigin(iconRect->getOrigin());
                icon->setPosition(iconRect->getPosition());
                if (icon->getLocalBounds().width != 32 || icon->getLocalBounds().height != 32) {
                    printf("ListBoxLine: icon size != 32, scaling icon...\n");
                    float scaling = std::min(
                            iconRect->getSize().y / icon->getLocalBounds().width,
                            iconRect->getSize().y / icon->getLocalBounds().height);
                    icon->setScale(scaling, scaling);
                }
            }
            add(icon);
            icon->setVisibility(icon->available ? Visibility::Visible : Visibility::Hidden);
            iconRect->setVisibility(icon->available ? Visibility::Hidden : Visibility::Visible);
        } else {
            iconRect->setVisibility(Visibility::Visible);
        }
    }
}

Text *UIListBoxLine::getText() {
    return text;
}

UIListBoxLine::~UIListBoxLine() {
    //printf("~ListBoxLine(%p)\n", this);
}

UIListBox::UIListBox(UiMain *_ui, Font *font, int fontSize, const FloatRect &rect,
                     const std::vector<ss_api::Game> &g) : RectangleShape(rect) {
    //printf("ListBox(%p)\n", this);
    ui = _ui;
    setGames(g);
    init(font, fontSize);
    setSelection(0);
};

void UIListBox::init(Font *font, int fontSize) {
    // set default bg colors
    setFillColor(Color::GrayLight);

    // calculate number of lines shown
    line_height = (float) fontSize + (2 * ui->getScaling().y);
    max_lines = (int) (getSize().y / line_height);
    if ((float) max_lines * line_height < getSize().y) {
        line_height = getSize().y / (float) max_lines;
    }

    // add selection rectangle (highlight)
    highlight = new RectangleShape(Vector2f(getSize().x - 2, line_height));
    add(highlight);

    // add lines of text
    for (unsigned int i = 0; i < (unsigned int) max_lines; i++) {
        FloatRect r = {0, line_height * (float) i, getSize().x, line_height};
        Texture *icon = nullptr;
        /* TODO: add back icon support ?
        if (use_icons) {
            icon = files.size() > i ? files[i]->icon : nullptr;
        }
        */
        auto *line = new UIListBoxLine(r, "W", font, (unsigned int) fontSize, ui->getScaling(), icon);
        lines.push_back(line);
        add(line);
    }
}

void UIListBox::updateLines() {
    bool useZipName = ui->getConfig()->getOption(PEMUConfig::OptId::UI_SHOW_ZIP_NAMES)->getInteger();

    for (unsigned int i = 0; i < (unsigned int) max_lines; i++) {
        if (file_index + i >= games.size()) {
            lines[i]->setVisibility(Visibility::Hidden);
        } else {
            // set file
            Game game = games[file_index + i];
            lines[i]->setVisibility(Visibility::Visible);
            lines[i]->setString(useZipName ? Utility::removeExt(game.path) : game.name);
            // TODO: ICON
            //lines[i]->setIcon(file->icon);
            lines[i]->setColor(game.available ? colorAvailable : colorMissing);
            // set highlight position and color
            if ((int) i == highlight_index) {
                highlight->setPosition(lines[i]->getPosition());
                highlight->move(1, 0);
                Color color = highlight_use_files_color ?
                              lines[i]->getText()->getFillColor() : highlight->getFillColor();
                color.a = highlight->getAlpha();
                highlight->setFillColor(color);
                color = highlight_use_files_color ?
                        lines[i]->getText()->getFillColor() : highlight->getOutlineColor();
                //color.a = highlight->getAlpha();
                highlight->setOutlineColor(color);
            }
        }
    }

    if (games.empty()) {
        highlight->setVisibility(Visibility::Hidden, false);
    } else {
        if (use_highlight) {
            highlight->setVisibility(Visibility::Visible, false);
        }
    }
}

void UIListBox::updateMarquee() {
    // 只让当前高亮行滚动；其余行保持原有显示。
    // setMarqueeEnabled 内部做了状态判断，重复调用不会反复重置滚动位置。
    for (unsigned int i = 0; i < (unsigned int) lines.size(); i++) {
        bool scroll = ((int) i == highlight_index) && !games.empty()
                      && (file_index + (int) i) < (int) games.size();
        lines[i]->setMarqueeEnabled(scroll);
        if (scroll) {
            lines[i]->updateMarquee();
        }
    }
}

void UIListBox::up() {
    int index = file_index + highlight_index;
    int middle = max_lines / 2;
    if (highlight_index <= middle && index - middle > 0) {
        file_index--;
    } else {
        highlight_index--;
    }
    if (highlight_index < 0) {
        highlight_index = (int) games.size() < max_lines ? (int) games.size() - 1 : max_lines - 1;
        file_index = (int) games.size() - 1 - highlight_index;
    }

    updateLines();
}

void UIListBox::down() {
    int index = file_index + highlight_index;
    int middle = max_lines / 2;
    if (highlight_index >= middle && index + middle < (int) games.size()) {
        file_index++;
    } else {
        highlight_index++;
    }
    if (highlight_index >= max_lines || file_index + highlight_index >= (int) games.size()) {
        file_index = 0;
        highlight_index = 0;
    }

    updateLines();
}

void UIListBox::setSelection(int new_index) {
    if (new_index < 0) new_index = 0;
    if (new_index > (int) games.size()) new_index = (int) games.size() - 1;
    if (new_index < max_lines / 2) {
        file_index = 0;
        highlight_index = 0;
    } else if (new_index > (int) games.size() - max_lines / 2) {
        highlight_index = max_lines - 1;
        file_index = (int) games.size() - 1 - highlight_index;
        if (highlight_index >= (int) games.size()) {
            highlight_index = (int) games.size() - 1;
            file_index = 0;
        }
    } else {
        highlight_index = max_lines / 2;
        file_index = new_index - highlight_index;
    }

    updateLines();
}

void UIListBox::setSize(const Vector2f &size) {
    UIListBox::setSize(size.x, size.y);
}

void UIListBox::setSize(float width, float height) {
    RectangleShape::setSize(width, height);
    highlight->setSize(width, highlight->getSize().y);
    for (auto &line: lines) {
        line->setSize(width, line->getSize().y);
    }
}

void UIListBox::setGames(const std::vector<ss_api::Game> &g) {
    games = g;

    if (!lines.empty()) {
        setSelection(0);
    }
}

std::vector<ss_api::Game> UIListBox::getGames() {
    return games;
}

Game UIListBox::getSelection() {
    if (!games.empty() && games.size() > (size_t) file_index + highlight_index) {
        return games[file_index + highlight_index];
    }
    return {};
}

std::vector<UIListBoxLine *> UIListBox::getLines() {
    return lines;
}

void UIListBox::setTextOutlineColor(const Color &color) {
    for (auto &line: lines) {
        line->getText()->setOutlineColor(color);
    }
}

void UIListBox::setTextOutlineThickness(float thickness) {
    for (auto &line: lines) {
        line->getText()->setOutlineThickness(thickness);
    }
}

void UIListBox::setHighlightEnabled(bool enable) {
    use_highlight = enable;
    highlight->setVisibility(enable ? Visibility::Visible
                                    : Visibility::Hidden);
}

void UIListBox::setHighlightUseFileColor(bool enable) {
    highlight_use_files_color = enable;
    setSelection(file_index);
}

int UIListBox::getIndex() {
    return file_index + highlight_index;
}

int UIListBox::getMaxLines() {
    return max_lines;
}

RectangleShape *UIListBox::getHighlight() {
    return highlight;
}

UIListBox::~UIListBox() {
    //printf("~ListBox(%p)\n", this);
}

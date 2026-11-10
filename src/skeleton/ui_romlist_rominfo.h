//
// Created by cpasjuste on 12/05/2020.
//

#ifndef PEMU_UI_ROMLIST_ROMINFO_H
#define PEMU_UI_ROMLIST_ROMINFO_H

#include "pemu.h"

class UIRomInfo : public c2d::Rectangle {

public:

    UIRomInfo(pemu::UiMain *u, pemu::UIRomList *uiRList, c2d::Font *fnt, int fntSize);

    ~UIRomInfo() override;

    c2d::Text *addInfoBoxText(const std::vector<std::string> &tree);

    void showText(c2d::Text *text, const std::string &msg);

    void hideText(c2d::Text *text);

    bool loadVideo(const ss_api::Game &game);

    bool loadTexture(const ss_api::Game &game);

    void load(const ss_api::Game &game = ss_api::Game());

    // 简介翻页（LB/RB）：把折行结果按 '\n' 切行，每游戏只计算一次行高与每页行数
    void setupSynopsisPages(const std::string &wrapped);

    void showSynopsisPage();

    bool pageUp();

    bool pageDown();

    pemu::UiMain *main = nullptr;
    pemu::UIRomList *uiRomList = nullptr;
    c2d::C2DTexture *texture = nullptr;
    c2d::Font *font;
    int fontSize;
    //
    c2d::RectangleShape *synoBox = nullptr;
    c2d::Text *synoText = nullptr;
    float synopsisWidth = 0; // 皮肤简介文本区宽度（ROM_SYNOPSIS TEXT rect.width），<=0 时不断行
    c2d::Text::Overflow synopsisOverflow = c2d::Text::NewLine; // 皮肤原始 overflow，英文/未折行时恢复用
    // 简介翻页状态
    std::vector<std::string> synopsisLines; // 折行后的完整行
    int synopsisLinesPerPage = 1;
    int synopsisPage = 0;
    bool synopsisPaged = false;             // 行数超过一屏时才允许翻页
    //
    c2d::RectangleShape *previewBox = nullptr;
    //
    c2d::RectangleShape *infoBox = nullptr;
    c2d::Text *systemText = nullptr;
    c2d::Text *developerText = nullptr;
    c2d::Text *editorText = nullptr;
    c2d::Text *dateText = nullptr;
    c2d::Text *genreText = nullptr;
    c2d::Text *playersText = nullptr;
    c2d::Text *ratingText = nullptr;
    c2d::Text *cloneofText = nullptr;
    c2d::Text *filenameText = nullptr;
#ifdef __MPV__
    Mpv *mpv = nullptr;
    MpvTexture *mpvTexture = nullptr;
#endif
};

#endif //PEMU_UI_ROMLIST_ROMINFO_H

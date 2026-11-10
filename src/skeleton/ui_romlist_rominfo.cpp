//
// Created by cpasjuste on 12/05/2020.
//

#include "pemu.h"
#include "text_wrap.h"

UIRomInfo::UIRomInfo(UiMain *u, UIRomList *uiRList, Font *fnt, int fntSize)
        : Rectangle(u->getSize()) {
    printf("UIRomInfo\n");
    main = u;
    uiRomList = uiRList;
    font = fnt;
    fontSize = fntSize;

    // synopsis box
    synoBox = new RectangleShape({16, 16});
    if (main->getSkin()->loadRectangleShape(synoBox, {"MAIN", "ROM_SYNOPSIS"})) {
        synoText = new Text("", (unsigned int) fontSize, font);
        main->getSkin()->loadText(synoText, {"MAIN", "ROM_SYNOPSIS", "TEXT"});
        // 记录皮肤为简介文本配置的可用宽度（与 loadText 内 setSizeMax 的宽度同源同值），供断行使用
        Skin::TextGroup tg = main->getSkin()->getText({"MAIN", "ROM_SYNOPSIS", "TEXT"});
        if (tg.available) {
            synopsisWidth = tg.rect.width;
            synopsisOverflow = tg.overflow;
        }
        synoBox->add(synoText);
        Rectangle::add(synoBox);
    } else {
        delete (synoBox);
    }

    // info box
    infoBox = new RectangleShape({16, 16});
    if (main->getSkin()->loadRectangleShape(infoBox, {"MAIN", "ROM_INFOS"})) {
        systemText = addInfoBoxText({"MAIN", "ROM_INFOS", "SYSTEM_TEXT"});
        developerText = addInfoBoxText({"MAIN", "ROM_INFOS", "DEVELOPER_TEXT"});
        editorText = addInfoBoxText({"MAIN", "ROM_INFOS", "EDITOR_TEXT"});
        dateText = addInfoBoxText({"MAIN", "ROM_INFOS", "DATE_TEXT"});
        genreText = addInfoBoxText({"MAIN", "ROM_INFOS", "GENRE_TEXT"});
        playersText = addInfoBoxText({"MAIN", "ROM_INFOS", "PLAYERS_TEXT"});
        ratingText = addInfoBoxText({"MAIN", "ROM_INFOS", "RATING_TEXT"});
        cloneofText = addInfoBoxText({"MAIN", "ROM_INFOS", "CLONEOF_TEXT"});
        filenameText = addInfoBoxText({"MAIN", "ROM_INFOS", "FILENAME_TEXT"});
        Rectangle::add(infoBox);
    } else {
        delete (infoBox);
    }

    // preview box
    previewBox = new SkinnedRectangle(main, {"SKIN_CONFIG", "MAIN", "ROM_IMAGE"});
    Rectangle::add(previewBox);

#ifdef __MPV__
    main->getIo()->create(main->getIo()->getDataPath() + "mpv");
    mpv = new Mpv(main->getIo()->getDataPath() + "mpv", true);
    mpvTexture = new MpvTexture({previewBox->getSize().x, previewBox->getSize().y}, mpv);
    mpvTexture->setOrigin(previewBox->getOrigin());
    mpvTexture->setPosition(previewBox->getPosition());
    mpvTexture->setAlpha(0);
    mpvTexture->setVisibility(Visibility::Hidden);
    mpvTexture->add(new TweenAlpha(0, 255, 0.3f, TweenLoop::None, TweenState::Playing));
    Rectangle::add(mpvTexture);
#endif
}

Text *UIRomInfo::addInfoBoxText(const std::vector<std::string> &tree) {
    Text *text = new Text("", (unsigned int) fontSize, font);
    if (main->getSkin()->loadText(text, tree)) {
        infoBox->add(text);
    } else {
        delete text;
        text = nullptr;
    }
    return text;
}

void UIRomInfo::showText(Text *text, const std::string &msg) {
    if (text != nullptr) {
        text->setString(msg);
        text->setVisibility(Visibility::Visible);
    }
}

void UIRomInfo::hideText(Text *text) {
    if (text != nullptr) {
        text->setVisibility(Visibility::Hidden);
        text->setString("");
    }
}

bool UIRomInfo::loadVideo(const Game &game) {
#ifdef __MPV__
    int res = -1;
    std::string videoPath = uiRomList->getPreview(game, UIRomList::PreviewType::Vid);
    if (!videoPath.empty()) {
        res = mpv->load(videoPath, Mpv::LoadType::Replace, "loop=yes");
        if (res == 0) {
            mpvTexture->setLayer(1);
            mpvTexture->setVisibility(Visibility::Visible, true);
            if (texture != nullptr) {
                texture->setVisibility(Visibility::Hidden, true);
            }
        }
    }
    if (res != 0) {
        if (mpvTexture->isVisible()) {
            mpvTexture->setVisibility(Visibility::Hidden, true);
        }
        mpv->stop();
    }
#endif
    return true;
}

bool UIRomInfo::loadTexture(const Game &game) {
    std::string path = game.path.empty() ? "" : uiRomList->getPreview(game, UIRomList::PreviewType::Tex);
    // set image
    if (!path.empty()) {
        delete (texture);
        texture = new C2DTexture(path);
        texture->setOrigin(Origin::Center);
        texture->setPosition(Vector2f(previewBox->getSize().x / 2, previewBox->getSize().y / 2));
        float tex_scaling = std::min(
                previewBox->getSize().x / (float) texture->getTextureRect().width,
                previewBox->getSize().y / (float) texture->getTextureRect().height);
        texture->setScale(tex_scaling, tex_scaling);
        texture->setAlpha(0);
        texture->add(new TweenAlpha(0, 255, 0.3f, TweenLoop::None, TweenState::Playing));
        previewBox->add(texture);
    } else {
        if (texture && texture->isVisible()) {
            texture->setVisibility(Visibility::Hidden, true);
        }
        return false;
    }

    return true;
}

void UIRomInfo::load(const Game &game) {
    if (game.id <= 0) {
        // try to load title/preview texture even if game is not in DB
        loadTexture(game);
        hideText(systemText);
        hideText(developerText);
        hideText(editorText);
        hideText(dateText);
        hideText(genreText);
        hideText(playersText);
        hideText(ratingText);
        hideText(cloneofText);
        hideText(filenameText);
        hideText(synoText);
        // 隐藏状态下清空翻页状态，避免残留上一游戏的页号
        synopsisLines.clear();
        synopsisLinesPerPage = 1;
        synopsisPage = 0;
        synopsisPaged = false;
#if __MPV__
        mpvTexture->setVisibility(Visibility::Hidden, true);
        mpv->stop();
#endif
    } else {
        printf("UIRomInfo::load(%s)\n", game.name.c_str());
        // load title/preview texture
        loadTexture(game);
        showText(systemText, pemu::tr("System: ") + game.system.name);
        showText(developerText, pemu::tr("Developer: ") + game.developer.name);
        showText(editorText, pemu::tr("Editor: ") + game.editor.name);
        showText(dateText, pemu::tr("Date: ") + game.date);
        showText(genreText, pemu::tr("Genre: ") + game.genre.name);
        showText(playersText, pemu::tr("Players: ") + game.players);
        showText(ratingText, pemu::tr("Rating: ") + std::to_string(game.rating));
        showText(cloneofText, pemu::tr("Clone Of: ") + (game.cloneOf.empty() ? pemu::tr("NONE") : game.cloneOf));
        showText(filenameText, pemu::tr("File: ") + game.path);
        // 中文断行：真正的简介只在光标停下后加载一次（见 UIRomList 的 mTimerLoadInfo），
        // 因此每游戏只计算一次；无 CJK 的文本原样返回，英文排版与改动前完全一致
        std::string synopsis = game.synopsis;
        if (synoText != nullptr) {
            if (pemu::hasCjkText(synopsis)) {
                synopsis = pemu::wrapText(font, synoText->getCharacterSize(), synopsis, synopsisWidth);
                // 中文简介整段没有空格，控件 NewLine 折行按空格分词后会把整段当成一个「词」，
                // 该「词」累计宽度必然超过文本框宽度，于是首行之前多换一行（=首行上方空一行）；
                // 单行时整段宽度不超限，所以不出现。断点已由 wrapText 按宽度插好，
                // 这里改用 Clamp：控件不再按词预折行，只按 '\n' 换行，单行/多行起始基线一致。
                synoText->setOverflow(Text::Clamp);
            } else {
                // 英文（wrapText 原样返回）：保持皮肤配置，逐行排版不变
                synoText->setOverflow(synopsisOverflow);
            }
        }
        // 简介翻页（LB/RB）：折行结果按 '\n' 切行，每游戏只算一次行高与每页行数，
        // 先显示第一页；不可翻页时（含英文）显示内容与上面原实现完全一致
        // 原实现（保留）：showText(synoText, synopsis);
        setupSynopsisPages(synopsis);
        showSynopsisPage();
    }
}

// 折行结果按 '\n' 切行；行高用与渲染同源的 findCharacterPos 度量（"X\nX" 两行之差），
// 每页行数 = 简介框可用高度 / 行高。只在 load() 里算一次，翻页时不再测量。
void UIRomInfo::setupSynopsisPages(const std::string &wrapped) {
    synopsisLines.clear();
    synopsisPage = 0;
    synopsisPaged = false;
    synopsisLinesPerPage = 1;

    // 保留空行，避免行号错位
    std::string line;
    for (size_t i = 0; i < wrapped.size(); i++) {
        if (wrapped[i] == '\n') {
            synopsisLines.push_back(line);
            line.clear();
        } else {
            line += wrapped[i];
        }
    }
    synopsisLines.push_back(line);

    if (synoText == nullptr || synoBox == nullptr) {
        return;
    }

    Text probe("X\nX", synoText->getCharacterSize(), font);
    float lineHeight = probe.findCharacterPos(2).y - probe.findCharacterPos(0).y;
    if (lineHeight <= 0) {
        return;
    }

    // 简介文本在框内从 rect.top 开始绘制，可用高度要扣掉这段上边距
    float top = synoText->getPosition().y > 0 ? synoText->getPosition().y : 0;
    int linesPerPage = (int) ((synoBox->getSize().y - top) / lineHeight);
    if (linesPerPage < 1) {
        linesPerPage = 1;
    }

    synopsisLinesPerPage = linesPerPage;
    synopsisPaged = (int) synopsisLines.size() > linesPerPage;
}

void UIRomInfo::showSynopsisPage() {
    if (synoText == nullptr) {
        return;
    }

    int total = (int) synopsisLines.size();
    int start = synopsisPage * synopsisLinesPerPage;
    if (start < 0 || start >= total) {
        start = 0;
        synopsisPage = 0;
    }

    std::string page;
    for (int i = 0; i < synopsisLinesPerPage && start + i < total; i++) {
        if (i > 0) {
            page += '\n';
        }
        page += synopsisLines[start + i];
    }
    showText(synoText, page);
}

bool UIRomInfo::pageUp() {
    if (!synopsisPaged || synopsisPage <= 0) {
        return false;
    }
    synopsisPage--;
    showSynopsisPage();
    return true;
}

bool UIRomInfo::pageDown() {
    if (!synopsisPaged) {
        return false;
    }
    int pages = ((int) synopsisLines.size() + synopsisLinesPerPage - 1) / synopsisLinesPerPage;
    if (synopsisPage + 1 >= pages) {
        return false;
    }
    synopsisPage++;
    showSynopsisPage();
    return true;
}

UIRomInfo::~UIRomInfo() {
    printf("~UIRomInfo\n");
#ifdef __MPV__
    delete (mpv);
#endif
}

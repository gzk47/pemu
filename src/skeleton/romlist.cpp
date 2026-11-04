//
// Created by cpasjuste on 19/10/16.
//

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include "pemu.h"
#include "romlist_cache.h"

RomList::RomList(UiMain *_ui, const std::string &emuVersion, const std::vector<std::string> &_filters) {
    printf("RomList()\n");
    ui = _ui;
    filters = _filters;

    // UI
    rect = new C2DRectangle(
            Vector2f(ui->getSize().x - 8, ui->getSize().y - 8));
    ui->getSkin()->loadRectangleShape(rect, {"MAIN"});

    auto *title = new RectangleShape({16, 16});
    if (ui->getSkin()->loadRectangleShape(title, {"MAIN", "TITLE"})) {
        title->setOrigin(Origin::Center);
        title->setPosition(Vector2f(rect->getSize().x / 2, rect->getSize().y / 2));
        float scaling = std::min(
                (rect->getSize().x * 0.8f) / title->getSize().x,
                (rect->getSize().y * 0.8f) / title->getSize().y);
        title->setScale(scaling, scaling);
        rect->add(title);
    } else {
        delete (title);
    }

    text = new Text();
    ui->getSkin()->loadText(text, {"ROM_LIST", "TEXT"});
    text->setOrigin(Origin::BottomLeft);
    text->setPosition(8, rect->getSize().y - 8);
    rect->add(text);

    auto *version = new Text();
    ui->getSkin()->loadText(version, {"ROM_LIST", "TEXT"});
    version->setOrigin(Origin::BottomRight);
    version->setPosition(rect->getSize().x - 8, rect->getSize().y - 8);
    version->setString(emuVersion);
    rect->add(version);

    ui->add(rect);
    //ui->flip();
    // UI

    gameList = new GameList();
    gameListFav = new GameList();
}

void RomList::build(const ss_api::GameList::GameAddedCb &cb) {
    float time_start = ui->getElapsedTime().asSeconds();
    auto cfg = ui->getConfig();
    p_cb = cb;
    m_games_count = m_games_available_count = 0;

    ui->flip();

    auto romPaths = cfg->getRomPaths();
    const std::string coreName = cfg->getName();
    for (const auto &p: romPaths) {
        std::string gameListPath = p.path + "gamelist.xml";
        const std::string cachePath = RomListCache::getCachePath(ui->getIo(), coreName, p.system.name, p.path);
        const size_t firstGame = gameList->games.size();
        uint32_t romFileCount = 0;
        uint64_t romNameHash = 0;
        RomListCache::getFingerprint(p.path, romFileCount, romNameHash);
        float path_start = ui->getElapsedTime().asSeconds();

        auto onGame = [this](Game *game) {
            if (p_cb) p_cb(game);
            m_games_count++;
            if (game->available) m_games_available_count++;
            if (!(m_games_count % 200)) {
                setLoadingText(pemu::tr("Games: %li / %li").c_str(), m_games_available_count, m_games_count);
            }
        };

        bool cacheHit = RomListCache::load(ui->getIo(), cachePath,
                                           coreName, p.system.name, p.system.id,
                                           gameListPath, p.path, romFileCount, romNameHash,
                                           gameList, onGame);
        float path_time = ui->getElapsedTime().asSeconds() - path_start;

        if (cacheHit) {
            // 命中不跑回调，这里只补计数器，保证进度文字与统计和全扫路径一致
            for (size_t i = firstGame; i < gameList->games.size(); i++) {
                m_games_count++;
                if (gameList->games[i].available) m_games_available_count++;
            }
            gameList->romPaths.emplace_back(p.path);
            gameList->xml = gameListPath;
            printf("RomList::build: %s: cache hit, %zu games in %f (system: %s (0x%08x), cache: %s)\n",
                   gameListPath.c_str(), gameList->games.size() - firstGame, path_time,
                   p.system.name.c_str(), p.system.id, cachePath.c_str());
        } else {
            bool ok = gameList->append(gameListPath, p.path, false, filters, p.system, false, onGame);
            path_time = ui->getElapsedTime().asSeconds() - path_start;
            printf("RomList::build: %s: full scan in %f (system: %s (0x%08x), cache: %s)\n",
                   gameListPath.c_str(), path_time, p.system.name.c_str(), p.system.id,
                   cachePath.c_str());
            // 目录非空且本次扫出至少一条才写缓存（与读侧对空目录判未命中对称）
            if (ok && romFileCount > 0 && gameList->games.size() > firstGame) {
                RomListCache::save(ui->getIo(), cachePath,
                                   coreName, p.system.name, p.system.id,
                                   gameListPath, p.path, romFileCount, romNameHash,
                                   *gameList, firstGame);
            }
        }

        setLoadingText(pemu::tr("Games: %li / %li").c_str(), m_games_available_count, m_games_count);
        printf("RomList::build: %s, games found: %zu / %zu (system: %s (0x%08x))\n",
               gameListPath.c_str(), gameList->getAvailableCount(),
               gameList->games.size(), p.system.name.c_str(), p.system.id);
    }

    // sort games
    printf("RomList::build: sorting games...\n");
    std::sort(gameList->systemList.systems.begin(), gameList->systemList.systems.end(), Api::sortSystemByName);
    std::sort(gameList->editors.begin(), gameList->editors.end(), Api::sortEditorByName);
    std::sort(gameList->developers.begin(), gameList->developers.end(), Api::sortDeveloperByName);
    std::sort(gameList->genres.begin(), gameList->genres.end(), Api::sortGenreByName);
    std::sort(gameList->players.begin(), gameList->players.end(), Api::sortInteger);
    std::sort(gameList->ratings.begin(), gameList->ratings.end(), Api::sortInteger);
    std::sort(gameList->rotations.begin(), gameList->rotations.end(), Api::sortInteger);
    std::sort(gameList->resolutions.begin(), gameList->resolutions.end(), Api::sortByName);
    std::sort(gameList->dates.begin(), gameList->dates.end(), Api::sortByName);

    // add filtering options
    printf("RomList::build: add filtering options...\n");
    auto grp = cfg->getGroup(PEMUConfig::GrpId::UI_FILTERING);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_SYSTEM)->setArray(gameList->systemList.getNames(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_GENRE)->setArray(gameList->getGenreNames(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_DATE)->setArray(gameList->getDates(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_EDITOR)->setArray(gameList->getEditorNames(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_DEVELOPER)->setArray(gameList->getDeveloperNames(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_PLAYERS)->setArray(gameList->getPlayersNames(), 0);
    grp->getOption(PEMUConfig::OptId::UI_FILTER_RATING)->setArray(gameList->getRatingNames(), 0);

    // custom core hide/show flags
    auto ids = cfg->getCoreHiddenOptionToEnable();
    for (const auto &id: ids) {
        cfg->get(id)->setFlags(0);
    }

    // we need to reload config to update new options we just added
    printf("RomList::build: reloading config...\n");
    cfg->load();

    printf("RomList::build(): list built in %f\n", ui->getElapsedTime().asSeconds() - time_start);

    // remove ui widgets
    delete (rect);
}

void RomList::initFav() {
    // load favorites
    delete (gameListFav);
    gameListFav = new GameList();
    gameListFav->append(ui->getIo()->getDataPath() + "favorites.xml");
    for (auto &g: gameListFav->games) {
        auto game = gameList->findGameByPathAndSystem(g.path, g.system.id);
        if (!game.path.empty()) {
            g.available = true;
            g.romsPath = game.romsPath;
        }
    }

    printf("RomList::initFav: %zu favorites\n", gameListFav->games.size());
}

void RomList::addFav(const Game &game) {
    if (!gameListFav->exist(game.id)) {
        gameListFav->games.emplace_back(game);
        gameListFav->save(ui->getIo()->getDataPath() + "favorites.xml", "mixrbv2", "", "video");
    }
}

void RomList::removeFav(const Game &game) {
    if (gameListFav->remove(game.id)) {
        gameListFav->save(ui->getIo()->getDataPath() + "favorites.xml", "mixrbv2", "", "video");
    }
}

void RomList::setLoadingText(const char *format, ...) {
    char buffer[512];
    va_list arg;
    va_start(arg, format);
    vsnprintf(buffer, 512, format, arg);
    va_end(arg);

    text->setString(buffer);
    ui->flip();
}

RomList::~RomList() {
    printf("~RomList()\n");
    delete (gameList);
    delete (gameListFav);
}

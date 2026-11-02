//
// Created by cpasjuste on 19/10/16.
//

#include <vector>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "pemu.h"

//
// ROM 列表全量结果缓存（单文件内联实现）
//
// 有效前提 = 两把指纹都对：
//   1. gamelist.xml 的大小 + mtime
//   2. ROM 目录的（文件数 + 按文件名排序后的 FNV-1a 哈希）
// 命中 → 反序列化直接显示；任何失败/损坏/读写失败 → 静默全扫，不弹窗。
//
namespace {

const char kCacheMagic[4] = {'P', 'R', 'L', 'C'};
const uint32_t kCacheVersion = 3;
const size_t kMaxCacheSize = 64 * 1024 * 1024; // 超限直接判无效，不读
const size_t kMinGameBytes = 20;               // 每条记录的最小字节数，用于 reserve 保护

// ---- 最小二进制 Reader/Writer（本机字节序，仅自用） ----

class CacheWriter {
public:
    void putBytes(const char *bytes, size_t size) { data.append(bytes, size); }
    void putU8(uint8_t v) { data.push_back((char) v); }
    void putU32(uint32_t v) { data.append((const char *) &v, sizeof(v)); }
    void putI32(int32_t v) { data.append((const char *) &v, sizeof(v)); }
    void putU64(uint64_t v) { data.append((const char *) &v, sizeof(v)); }
    void putI64(int64_t v) { data.append((const char *) &v, sizeof(v)); }
    void putStr(const std::string &s) {
        putU32((uint32_t) s.size());
        if (!s.empty()) data.append(s);
    }
    std::string data;
};

class CacheReader {
public:
    CacheReader(const char *d, size_t s) : data(d), size(s) {}

    bool getU8(uint8_t &v) { return read(&v, 1); }
    bool getU32(uint32_t &v) { return read(&v, sizeof(v)); }
    bool getI32(int32_t &v) { return read(&v, sizeof(v)); }
    bool getU64(uint64_t &v) { return read(&v, sizeof(v)); }
    bool getI64(int64_t &v) { return read(&v, sizeof(v)); }
    bool getBytes(char *out, size_t n) { return read(out, n); }
    bool getStr(std::string &s) {
        uint32_t n = 0;
        if (!getU32(n) || n > size) return false;
        if (!has(n)) return false;
        s.assign(data + pos, n);
        pos += n;
        return true;
    }
    bool done() const { return pos == size; }

private:
    bool has(size_t n) const { return pos <= size && n <= size - pos; }
    bool read(void *out, size_t n) {
        if (!has(n)) return false;
        memcpy(out, data + pos, n);
        pos += n;
        return true;
    }
    const char *data;
    size_t size;
    size_t pos = 0;
};

// ---- 指纹计算 ----

struct RomDirFingerprint {
    uint32_t fileCount = 0;
    uint64_t nameHash = 0;
};

bool getXmlStat(const std::string &xmlPath, uint64_t &size, int64_t &mtime) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (::stat(xmlPath.c_str(), &st) != 0) return false;
    size = (uint64_t) st.st_size;
    mtime = (int64_t) st.st_mtime;
    return true;
}

// 目录指纹：只读文件名，不逐项 stat（Io::getDirList 在部分平台会 stat，命中路径的耗时就靠这一条省掉）
RomDirFingerprint getRomDirFingerprint(const std::string &path) {
    std::vector<std::string> names;
    DIR *dir = opendir(path.c_str());
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.') continue; // 跳过 "." ".." 和隐藏文件
            names.emplace_back(ent->d_name);
        }
        closedir(dir);
    }
    std::sort(names.begin(), names.end());
    uint64_t hash = 1469598103934665603ULL; // FNV-1a 64
    for (const auto &n: names) {
        for (char c: n) {
            hash ^= (unsigned char) c;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xff;
        hash *= 1099511628211ULL; // 条目分隔符
    }
    return {(uint32_t) names.size(), hash};
}

std::string sanitizeCacheName(const std::string &coreName, const std::string &systemName,
                              const std::string &romPath) {
    std::string name = coreName + "_" + systemName;
    for (auto &c: name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) c = '_';
    }
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c: romPath) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char suffix[24];
    snprintf(suffix, sizeof(suffix), "_%016llx", (unsigned long long) h);
    return name + suffix;
}

// ---- Game 序列化 ----

void writeGame(CacheWriter &w, const ss_api::Game &g) {
    w.putU64((uint64_t) g.id);
    w.putI32((int32_t) g.rating);
    w.putI32((int32_t) g.rotation);
    w.putU8(g.available ? 1 : 0);
    w.putStr(g.cloneOf);
    w.putStr(g.players);
    w.putI32((int32_t) g.playersInt);
    w.putStr(g.resolution);
    w.putStr(g.system.name);
    w.putI32((int32_t) g.system.id);
    w.putI32((int32_t) g.system.parentId);
    w.putI32((int32_t) g.editor.id);
    w.putStr(g.editor.name);
    w.putI32((int32_t) g.developer.id);
    w.putStr(g.developer.name);
    w.putI32((int32_t) g.genre.id);
    w.putStr(g.genre.name);
    w.putStr(g.name);
    w.putStr(g.synopsis);
    w.putStr(g.date);
    w.putStr(g.path);
    w.putStr(g.romsPath);
    w.putU32((uint32_t) g.medias.size());
    for (const auto &m: g.medias) {
        w.putStr(m.url);
        w.putStr(m.type);
        w.putStr(m.format);
    }
}

bool readGame(CacheReader &r, ss_api::Game &g) {
    uint64_t id = 0;
    int32_t rating = 0, rotation = 0, playersInt = 0;
    uint8_t available = 0;
    if (!r.getU64(id) || !r.getI32(rating) || !r.getI32(rotation) || !r.getU8(available)) return false;
    g.id = (unsigned long) id;
    g.rating = rating;
    g.rotation = rotation;
    g.available = available != 0;
    if (!r.getStr(g.cloneOf) || !r.getStr(g.players) || !r.getI32(playersInt)) return false;
    g.playersInt = playersInt;
    if (!r.getStr(g.resolution)) return false;
    if (!r.getStr(g.system.name) || !r.getI32(g.system.id) || !r.getI32(g.system.parentId)) return false;
    if (!r.getI32(g.editor.id) || !r.getStr(g.editor.name)) return false;
    if (!r.getI32(g.developer.id) || !r.getStr(g.developer.name)) return false;
    if (!r.getI32(g.genre.id) || !r.getStr(g.genre.name)) return false;
    if (!r.getStr(g.name) || !r.getStr(g.synopsis) || !r.getStr(g.date)) return false;
    if (!r.getStr(g.path) || !r.getStr(g.romsPath)) return false;
    uint32_t mediaCount = 0;
    if (!r.getU32(mediaCount) || mediaCount > 1000) return false;
    g.medias.clear();
    g.medias.resize(mediaCount);
    for (auto &m: g.medias) {
        if (!r.getStr(m.url) || !r.getStr(m.type) || !r.getStr(m.format)) return false;
    }
    return true;
}

// ---- 聚合列表：不序列化，从 games 重算 ----
// 合并规则与 sscrap GameList::append 一致（id 或 name 相同即同一条），避免旧目录幽灵项。

void mergeSystem(ss_api::SystemList &list, const ss_api::System &v) {
    for (const auto &x: list.systems) { if (x.id == v.id || x.name == v.name) return; }
    list.systems.emplace_back(v);
}

void mergeEditor(std::vector<ss_api::Game::Editor> &list, const ss_api::Game::Editor &v) {
    for (const auto &x: list) { if (x.id == v.id || x.name == v.name) return; }
    list.emplace_back(v);
}

void mergeDeveloper(std::vector<ss_api::Game::Developer> &list, const ss_api::Game::Developer &v) {
    for (const auto &x: list) { if (x.id == v.id || x.name == v.name) return; }
    list.emplace_back(v);
}

void mergeGenre(std::vector<ss_api::Game::Genre> &list, const ss_api::Game::Genre &v) {
    for (const auto &x: list) { if (x.id == v.id || x.name == v.name) return; }
    list.emplace_back(v);
}

void mergeInt(std::vector<int> &list, int v) {
    for (int x: list) { if (x == v) return; }
    list.emplace_back(v);
}

void mergeString(std::vector<std::string> &list, const std::string &v) {
    for (const auto &x: list) { if (x == v) return; }
    list.emplace_back(v);
}

void rebuildAggregates(ss_api::GameList &list) {
    list.systemList.systems.clear();
    list.editors.clear();
    list.developers.clear();
    list.genres.clear();
    list.players.clear();
    list.ratings.clear();
    list.rotations.clear();
    list.resolutions.clear();
    list.dates.clear();
    for (const auto &g: list.games) {
        mergeSystem(list.systemList, g.system);
        mergeEditor(list.editors,
                    g.editor.name.empty() ? ss_api::Game::Editor{0, "UNKNOWN"} : g.editor);
        mergeDeveloper(list.developers,
                       g.developer.name.empty() ? ss_api::Game::Developer{0, "UNKNOWN"} : g.developer);
        mergeGenre(list.genres,
                   g.genre.name.empty() ? ss_api::Game::Genre{0, "UNKNOWN"} : g.genre);
        mergeInt(list.players, g.playersInt);
        mergeInt(list.ratings, g.rating);
        mergeInt(list.rotations, g.rotation);
        mergeString(list.resolutions, g.resolution.empty() ? "UNKNOWN" : g.resolution);
        mergeString(list.dates, g.date);
    }
}

} // namespace

// ---- 读写入口（内联静态） ----

static bool RomListCacheLoad(c2d::Io *io, const std::string &cachePath,
                             const std::string &coreName,
                             const std::string &systemName, int systemId,
                             const std::string &xmlPath, const std::string &romPath,
                             const RomDirFingerprint &fp,
                             ss_api::GameList *list,
                             const std::function<void(ss_api::Game *)> &cb) {
    if (io == nullptr || list == nullptr) return false;

    // 指纹检查：gamelist.xml
    uint64_t xmlSize = 0;
    int64_t xmlMtime = 0;
    // 没有 gamelist.xml 的路径（rom 直接放在目录里）：用 (0,0) 作为“无 xml”指纹继续走缓存。
    // 否则这类路径的 save 也会在同一个 stat 上退出，缓存永远不会存在，每次启动都必须全扫。
    // xml 出现或消失时指纹从 0 变成实际值（或反之），会自然失效并重扫一次。
    if (!getXmlStat(xmlPath, xmlSize, xmlMtime)) {
        xmlSize = 0;
        xmlMtime = 0;
    }

    if (fp.fileCount == 0) {
        printf("RomListCacheLoad: empty rom dir: %s\n", romPath.c_str());
        return false; // 空目录，走全扫
    }

    if (!io->exist(cachePath)) return false;
    const size_t cacheSize = io->getSize(cachePath);
    if (cacheSize == 0 || cacheSize > kMaxCacheSize) return false;

    std::vector<char> buf(cacheSize);
    const size_t readSize = io->read(cachePath, buf.data(), cacheSize);
    if (readSize != cacheSize) {
        printf("RomListCacheLoad: read size mismatch: %zu/%zu: %s\n",
               readSize, cacheSize, cachePath.c_str());
        return false;
    }

    CacheReader r(buf.data(), cacheSize);
    char magic[4] = {0};
    if (!r.getBytes(magic, 4) || memcmp(magic, kCacheMagic, 4) != 0) return false;
    uint32_t version = 0;
    if (!r.getU32(version) || version != kCacheVersion) return false;

    std::string cachedCore, cachedSystem, cachedRomPath;
    int32_t cachedSystemId = 0;
    if (!r.getStr(cachedCore) || !r.getStr(cachedSystem) || !r.getI32(cachedSystemId) ||
        !r.getStr(cachedRomPath)) {
        return false;
    }
    if (cachedCore != coreName || cachedSystem != systemName ||
        cachedSystemId != systemId || cachedRomPath != romPath) {
        printf("RomListCacheLoad: identity mismatch: core=%s/%s sys=%s/%s id=0x%08x/0x%08x path=%s/%s\n",
               cachedCore.c_str(), coreName.c_str(), cachedSystem.c_str(), systemName.c_str(),
               (unsigned int) cachedSystemId, (unsigned int) systemId,
               cachedRomPath.c_str(), romPath.c_str());
        return false;
    }

    uint64_t cachedXmlSize = 0, cachedNameHash = 0;
    int64_t cachedXmlMtime = 0;
    uint32_t cachedFileCount = 0;
    if (!r.getU64(cachedXmlSize) || !r.getI64(cachedXmlMtime) ||
        !r.getU32(cachedFileCount) || !r.getU64(cachedNameHash)) {
        return false;
    }
    if (cachedXmlSize != xmlSize || cachedXmlMtime != xmlMtime) {
        printf("RomListCacheLoad: xml fingerprint mismatch: size=%llu/%llu mtime=%lld/%lld\n",
               (unsigned long long) cachedXmlSize, (unsigned long long) xmlSize,
               (long long) cachedXmlMtime, (long long) xmlMtime);
        return false;
    }
    if (cachedFileCount != fp.fileCount || cachedNameHash != fp.nameHash) {
        printf("RomListCacheLoad: dir fingerprint mismatch: count=%u/%u hash=%016llx/%016llx\n",
               (unsigned int) cachedFileCount, (unsigned int) fp.fileCount,
               (unsigned long long) cachedNameHash, (unsigned long long) fp.nameHash);
        return false;
    }

    // 预分配保护：按文件实际大小估算合理上限，防止损坏头部谎报条数导致巨额 reserve
    uint32_t gameCount = 0;
    if (!r.getU32(gameCount)) return false;
    if (gameCount > 0) {
        const size_t maxReasonable = cacheSize / kMinGameBytes;
        if ((size_t) gameCount > maxReasonable) return false;
    }

    std::vector<ss_api::Game> games;
    games.reserve(gameCount);
    for (uint32_t i = 0; i < gameCount; i++) {
        ss_api::Game g;
        if (!readGame(r, g)) return false;
        games.emplace_back(std::move(g));
    }
    if (!r.done()) return false; // 尾部有多余字节

    // 缓存里的记录就是“上次全扫 + 回调之后”的状态（system/cloneOf/editor/name 已由回调解析），
    // 命中时再逐条回调是纯重复工作（pfbneo 每条都要线性查一次驱动表），这里直接入库。
    // 回调对 gameList->systemList 的副作用由下面的 rebuildAggregates 等价重建；
    // 计数（进度文字/统计）由 build() 的命中分支补上，保持与全扫一致。
    (void) cb;
    const size_t firstGame = list->games.size();
    list->games.reserve(firstGame + games.size());
    for (auto &g: games) {
        list->games.emplace_back(std::move(g));
    }
    rebuildAggregates(*list);
    return true;
}

static void RomListCacheSave(c2d::Io *io, const std::string &cachePath,
                             const std::string &coreName,
                             const std::string &systemName, int systemId,
                             const std::string &xmlPath, const std::string &romPath,
                             const RomDirFingerprint &fp,
                             const ss_api::GameList &list, size_t firstGame) {
    if (io == nullptr) return;

    const std::string cacheDir = io->getDataPath() + "cache/";
    if (!io->exist(cacheDir)) {
        io->create(cacheDir);
        if (!io->exist(cacheDir)) return; // 建目录失败，静默放弃
    }

    uint64_t xmlSize = 0;
    int64_t xmlMtime = 0;
    if (!getXmlStat(xmlPath, xmlSize, xmlMtime)) {
        xmlSize = 0;
        xmlMtime = 0; // 无 gamelist.xml 的路径同样可以缓存，指纹记 (0,0)
    }

    CacheWriter w;
    w.putBytes(kCacheMagic, 4);
    w.putU32(kCacheVersion);
    w.putStr(coreName);
    w.putStr(systemName);
    w.putI32((int32_t) systemId);
    w.putStr(romPath);
    w.putU64(xmlSize);
    w.putI64(xmlMtime);
    w.putU32(fp.fileCount);
    w.putU64(fp.nameHash);

    const size_t count = list.games.size() > firstGame ? list.games.size() - firstGame : 0;
    w.putU32((uint32_t) count);
    for (size_t i = firstGame; i < list.games.size(); i++) {
        writeGame(w, list.games[i]);
    }

    // 写 tmp → 完整写好后删除旧文件 → rename 就位；失败静默清理
    const std::string tmpPath = cachePath + ".tmp";
    io->removeFile(tmpPath);
    if (!io->write(tmpPath, w.data.data(), w.data.size())) {
        io->removeFile(tmpPath);
        return;
    }
    io->removeFile(cachePath);
    if (std::rename(tmpPath.c_str(), cachePath.c_str()) != 0) {
        io->removeFile(tmpPath);
        return;
    }
}

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
        const std::string cacheName = sanitizeCacheName(coreName, p.system.name, p.path);
        const std::string cachePath = ui->getIo()->getDataPath() + "cache/romlist_" + cacheName + ".cache";
        const size_t firstGame = gameList->games.size();
        // 目录指纹在这里只算一次，命中判断与写缓存条件共用，避免重复遍历目录
        const RomDirFingerprint dirFp = getRomDirFingerprint(p.path);
        float path_start = ui->getElapsedTime().asSeconds();

        auto onGame = [this](Game *game) {
            if (p_cb) p_cb(game);
            m_games_count++;
            if (game->available) m_games_available_count++;
            if (!(m_games_count % 200)) {
                setLoadingText(pemu::tr("Games: %li / %li").c_str(), m_games_available_count, m_games_count);
            }
        };

        bool cacheHit = RomListCacheLoad(ui->getIo(), cachePath,
                                         coreName, p.system.name, p.system.id,
                                         gameListPath, p.path, dirFp, gameList, onGame);
        float path_time = ui->getElapsedTime().asSeconds() - path_start;

        if (cacheHit) {
            // 命中不再走 onGame/p_cb（见 RomListCacheLoad），这里只补计数器，
            // 保证进度文字与最终统计和全扫路径一致。
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
            // 只有“目录非空 且 本次扫出至少一条游戏”才写缓存：
            // 读缓存一侧对空目录直接判未命中，写一侧不写才对称；
            // 否则空目录（或全被过滤）每次启动都会白重写一遍缓存，Arcade 那种要重写整个 5.6MB。
            if (ok && dirFp.fileCount > 0 && gameList->games.size() > firstGame) {
                RomListCacheSave(ui->getIo(), cachePath,
                                 coreName, p.system.name, p.system.id,
                                 gameListPath, p.path, dirFp, *gameList, firstGame);
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

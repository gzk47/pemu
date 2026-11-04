//
// ROM 列表缓存：外联实现
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
#include "romlist_cache.h"

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

bool getXmlStat(const std::string &xmlPath, uint64_t &size, int64_t &mtime) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (::stat(xmlPath.c_str(), &st) != 0) return false;
    size = (uint64_t) st.st_size;
    mtime = (int64_t) st.st_mtime;
    return true;
}

// 目录指纹：只读文件名，不逐项 stat（Io::getDirList 在部分平台会 stat，命中路径的耗时就靠这一条省掉）
void getRomDirFingerprint(const std::string &path, uint32_t &fileCount, uint64_t &nameHash) {
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
    fileCount = (uint32_t) names.size();
    nameHash = hash;
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

std::string RomListCache::getCachePath(c2d::Io *io, const std::string &coreName,
                                       const std::string &systemName, const std::string &romPath) {
    return io->getDataPath() + "cache/romlist_" +
           sanitizeCacheName(coreName, systemName, romPath) + ".cache";
}

void RomListCache::getFingerprint(const std::string &romPath, uint32_t &fileCount, uint64_t &nameHash) {
    getRomDirFingerprint(romPath, fileCount, nameHash);
}

bool RomListCache::load(c2d::Io *io, const std::string &cachePath,
                        const std::string &coreName,
                        const std::string &systemName, int systemId,
                        const std::string &xmlPath, const std::string &romPath,
                        uint32_t romFileCount, uint64_t romNameHash,
                        ss_api::GameList *list,
                        const std::function<void(ss_api::Game *)> &cb) {
    if (io == nullptr || list == nullptr) return false;

    // 指纹检查：gamelist.xml
    uint64_t xmlSize = 0;
    int64_t xmlMtime = 0;
    // 没有 gamelist.xml 的路径（rom 直接放在目录里）：用 (0,0) 作为“无 xml”指纹继续走缓存。
    // xml 出现或消失时指纹从 0 变成实际值（或反之），会自然失效并重扫一次。
    if (!getXmlStat(xmlPath, xmlSize, xmlMtime)) {
        xmlSize = 0;
        xmlMtime = 0;
    }

    if (romFileCount == 0) {
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
    if (cachedFileCount != romFileCount || cachedNameHash != romNameHash) {
        printf("RomListCacheLoad: dir fingerprint mismatch: count=%u/%u hash=%016llx/%016llx\n",
               (unsigned int) cachedFileCount, (unsigned int) romFileCount,
               (unsigned long long) cachedNameHash, (unsigned long long) romNameHash);
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

    // 缓存里的记录就是“上次全扫 + 回调之后”的状态，命中时直接入库（不重跑核心回调）；
    // 回调对 gameList->systemList 的副作用由 rebuildAggregates 等价重建，
    // 计数由 build() 的命中分支补上，保持与全扫一致。
    (void) cb;
    const size_t firstGame = list->games.size();
    list->games.reserve(firstGame + games.size());
    for (auto &g: games) {
        list->games.emplace_back(std::move(g));
    }
    rebuildAggregates(*list);
    return true;
}

void RomListCache::save(c2d::Io *io, const std::string &cachePath,
                        const std::string &coreName,
                        const std::string &systemName, int systemId,
                        const std::string &xmlPath, const std::string &romPath,
                        uint32_t romFileCount, uint64_t romNameHash,
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
    w.putU32(romFileCount);
    w.putU64(romNameHash);

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

//
// ROM 列表缓存：外联实现（供 romlist.cpp 调用）
//
// 有效前提 = 两把指纹都对：
//   1. gamelist.xml 的大小 + mtime
//   2. ROM 目录的（文件数 + 按文件名排序后的 FNV-1a 哈希）
// 命中 → 反序列化直接显示；任何失败/损坏/读写失败 → 静默全扫，不弹窗。
//
#ifndef PEMU_ROMLIST_CACHE_H
#define PEMU_ROMLIST_CACHE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include "ss_api.h"

namespace c2d {
    class Io;
}

namespace RomListCache {

// 缓存路径：<数据目录>/cache/romlist_<核心名>_<系统名>_<rom路径哈希>.cache
// 核心名/系统名中非字母数字 -> '_'
std::string getCachePath(c2d::Io *io, const std::string &coreName,
                         const std::string &systemName, const std::string &romPath);

// ROM 目录指纹：只读文件名（不逐项 stat），排序后做 FNV-1a 64
void getFingerprint(const std::string &romPath, uint32_t &fileCount, uint64_t &nameHash);

// 尝试读缓存并追加到 list；任何校验失败返回 false（调用方回退全量扫描）
bool load(c2d::Io *io, const std::string &cachePath,
          const std::string &coreName, const std::string &systemName, int systemId,
          const std::string &xmlPath, const std::string &romPath,
          uint32_t romFileCount, uint64_t romNameHash,
          ss_api::GameList *list,
          const std::function<void(ss_api::Game *)> &cb);

// 全量扫描成功后写入 list.games[firstGame..]；失败静默放弃
void save(c2d::Io *io, const std::string &cachePath,
          const std::string &coreName, const std::string &systemName, int systemId,
          const std::string &xmlPath, const std::string &romPath,
          uint32_t romFileCount, uint64_t romNameHash,
          const ss_api::GameList &list, size_t firstGame);

}

#endif //PEMU_ROMLIST_CACHE_H

//
// gamelist 编码处理：GBK -> UTF-8（内置映射表，不依赖 iconv）
//

#ifndef PEMU_ENCODING_H
#define PEMU_ENCODING_H

#include <string>

namespace pemu {
    namespace encoding {

        // 严格校验文件是否为合法 UTF-8（流式读取，不整体载入内存）
        bool isUtf8File(const std::string &path);

        /*
         * 若文件已是合法 UTF-8（或无法打开），返回空串，调用方应直接使用原文件。
         * 否则按 GBK 转码写入 tmpPath 并返回 tmpPath；调用方解析完成后负责删除该临时文件。
         * 转码失败时返回空串，并清除残留的临时文件。
         */
        std::string createUtf8CopyIfNeeded(const std::string &path, const std::string &tmpPath);
    }
}

#endif //PEMU_ENCODING_H

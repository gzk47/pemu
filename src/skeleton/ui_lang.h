//
// 界面中文本地化，集中管理表
//

#ifndef PEMU_UI_LANG_H
#define PEMU_UI_LANG_H

#include <string>

namespace pemu {
    std::string tr(const std::string &key);
    std::string trValue(const std::string &value);
}

#endif //PEMU_UI_LANG_H

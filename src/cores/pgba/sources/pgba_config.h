//
// Created by cpasjuste on 29/05/18.
//

#ifndef PGBA_CONFIG_H
#define PGBA_CONFIG_H

#include "skeleton/pemu_config.h"
extern "C" {
#include "mgba/core/version.h"
}

class PGBAConfig final : public PEMUConfig {
public:
    PGBAConfig(Renderer *renderer, int version);

    std::string getCoreVersion() override {
        return std::string(projectName) + projectVersion;
    }

    std::vector<int> getCoreHiddenOptionToEnable() override {
        return {
            UI_FILTER_SYSTEM
        };
    }

    std::vector<std::string> getCoreSupportedExt() override {
        return {".zip", ".gba", ".gb", ".gbc", ".bin"};
    }
};

#endif //PGBA_CONFIG_H

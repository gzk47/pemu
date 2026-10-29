//
// 界面中文本地化，集中管理表
//

#include "pemu.h"

#include <cstddef>

namespace {

    struct Entry {
        const char *key;
        const char *text;
    };

    const Entry kStaticText[] = {
            // 主菜单 / 分组标题 / 特殊项
            {"MAIN OPTIONS", "主菜单"},
            {"UI_FILTERING", "界面过滤"},
            {"UI_OPTIONS", "界面设置"},
            {"EMULATION", "模拟设置"},
            {"GAMEPAD", "手柄设置"},
            {"KEYBOARD", "键盘设置"},
            {"OTHER", "其他"},
            {"STATES", "即时存档"},
            {"QUIT", "退出"},
            {"GO", "执行"},
            {"NEW INPUT", "重新设定按键"},
            {"PRESS A BUTTON", "请按一个按键"},

            // 界面过滤
            {"FILTER_FAVORITES", "收藏夹"},
            {"FILTER_MISSING", "隐藏缺失 ROM"},
            {"FILTER_CLONES", "隐藏克隆版"},
            {"FILTER_SYSTEM", "系统"},
            {"FILTER_GENRE", "类型"},
            {"FILTER_DATE", "年份"},
            {"FILTER_EDITOR", "发行商"},
            {"FILTER_DEVELOPER", "开发商"},
            {"FILTER_PLAYERS", "玩家数"},
            {"FILTER_RATING", "评分"},

            // 界面设置
            {"SHOW_ZIP_NAMES", "显示 ROM 文件名"},
            {"FULLSCREEN", "全屏"},
            {"SKIN_ASPECT", "屏幕比例"},
            {"FONT_SCALING", "字体缩放"},
            {"VIDEO_SNAP_DELAY", "预览延迟（秒）"},
            {"SKIN", "皮肤"},

            // 模拟设置
            {"SCALING", "缩放"},
            {"SCALING_MODE", "缩放模式"},
            {"FILTER", "纹理过滤"},
            {"EFFECT", "着色器效果"},
            {"WAIT_RENDERING", "等待渲染"},
            {"SHOW_FPS", "显示帧率"},
            {"AUDIO_SYNC", "音频同步"},
            {"CHEATS", "金手指"},
            {"BLOCk_INVALID_VRAM", "屏蔽无效 VRAM"},
            {"TRANSPARENCY", "透明效果"},
            {"DISPLAY_MESSAGES", "显示核心消息"},
            {"FRAMESKIP", "跳帧"},
            {"TURBO_MODE", "加速模式"},
            {"TURBO_FRAMESKIP", "加速跳帧"},
            {"FORCE_60HZ", "强制 60Hz"},
            {"AUDIO_FREQUENCY", "音频采样率"},
            {"AUDIO_INTERPOLATION", "音频插值"},
            {"AUDIO_FM_INTERPOLATION", "FM 音频插值"},
            {"ROTATION", "屏幕旋转"},
            {"NEOBIOS", "NeoGeo BIOS"},

            // 手柄设置
            {"JOY_UP", "上"},
            {"JOY_DOWN", "下"},
            {"JOY_LEFT", "左"},
            {"JOY_RIGHT", "右"},
            {"JOY_A", "A"},
            {"JOY_B", "B"},
            {"JOY_X", "X"},
            {"JOY_Y", "Y"},
            {"JOY_LT", "LT"},
            {"JOY_RT", "RT"},
            {"JOY_LB", "LB"},
            {"JOY_RB", "RB"},
            {"JOY_SELECT", "Select"},
            {"JOY_START", "Start"},
            {"JOY_MENU1", "菜单 1"},
            {"JOY_MENU2", "菜单 2"},
            {"JOY_DEADZONE", "摇杆死区"},

            // 键盘设置
            {"KEY_UP", "上"},
            {"KEY_DOWN", "下"},
            {"KEY_LEFT", "左"},
            {"KEY_RIGHT", "右"},
            {"KEY_A", "A"},
            {"KEY_B", "B"},
            {"KEY_X", "X"},
            {"KEY_Y", "Y"},
            {"KEY_LT", "LT"},
            {"KEY_RT", "RT"},
            {"KEY_LB", "LB"},
            {"KEY_RB", "RB"},
            {"KEY_SELECT", "Select"},
            {"KEY_START", "Start"},
            {"KEY_MENU1", "菜单 1"},
            {"KEY_MENU2", "菜单 2"},

            // 选项注释（状态栏显示）
            {"YOU NEED TO RESTART THE APPLICATION AFTER CHANGING THIS OPTION", "修改此选项后需重启应用"},
            {"YOU NEED TO RESTART EMULATION AFTER CHANGING THIS OPTION", "修改此选项后需重启模拟"},
            {"ON: PERFECT AUDIO - OFF: MINOR AUDIO STUTTERING (FAVOR FPS)", "开：音频完美；关：轻微爆音（优先帧率）"},

            // 帮助栏
            {"NAVIGATION", "导航"},
            {"RUN", "运行"},
            {"ADD / REMOVE FAVORITE", "添加/移除收藏"},
            {"FAVORITE", "收藏"},
            {"SWITCH SYSTEM", "切换系统"},
            {"MAIN MENU", "主菜单"},
            {"ROM MENU", "游戏菜单"},

            // 状态栏 / 帧率 / 缩放提示
            {"TIPS:", "提示："},
            {"TIPS: PRESS MENU1 + MENU2 BUTTONS FOR IN GAME MENU...", "提示：同时按 MENU1 + MENU2 打开游戏内菜单…"},
            {"FPS: 00/60", "帧率：00/60"},
            {"FPS: %.1f/%.1f", "帧率：%.1f/%.1f"},
            {"GAME: %ix%i - RATIO: %.2f | OUTPUT: %ix%i - RATIO: %.2f - SCALING: %.2fx%.2f",
                    "游戏：%ix%i - 比例：%.2f | 输出：%ix%i - 比例：%.2f - 缩放：%.2fx%.2f"},
            {"TRY TO KEEP INTEGER SCALING IF ASPECT RATIO IS NOT TOO DIVERGENT", "尽量保持整数倍缩放（比例差异不大时）"},
            {"KEEP GAME ASPECT RATIO - SOME SHADERS MAY NOT RENDER CORRECTLY", "保持游戏原始比例（部分着色器可能显示异常）"},
            {"FORCE INTEGER SCALING - ASPECT RATIO MAY BE WRONG BUT SHADERS WILL RENDER CORRECTLY",
                    "强制整数倍缩放（比例可能失真，但着色器显示正常）"},

            // 游戏列表 / 进度 / 收藏确认
            {"Games: %li / %li", "游戏：%li / %li"},
            {"FAVORITES", "收藏"},
            {"Add to favorites ?", "加入收藏？"},
            {"Remove from favorites ?", "移除收藏？"},
            {"OK", "确定"},
            {"CANCEL", "取消"},

            // ROM 信息标签
            {"System: ", "系统："},
            {"Developer: ", "开发商："},
            {"Editor: ", "发行商："},
            {"Date: ", "日期："},
            {"Genre: ", "类型："},
            {"Players: ", "玩家人数："},
            {"Rating: ", "评分："},
            {"Clone Of: ", "克隆自："},
            {"File: ", "文件："},
            {"NONE", "无"},

            // 即时存档菜单
            {"NO SAVE", "无存档"},
            {"NO PIC", "无截图"},
            {"SLOT %i", "存档位 %i"},
            {"PRESS FIRE2 TO CANCEL", "按 FIRE2 取消"},
            {"LOAD", "读取"},
            {"SAVE", "保存"},

            // 进度 / 提示 / 错误弹窗
            {"Please wait...", "请稍候…"},
            {"ERROR", "错误"},
            {"WARNING", "警告"},
            {"INVALID FILE", "文件无效"},
            {"INVALID ROM", "ROM 无效"},
            {"INVALID ROM FILE", "ROM 文件无效"},
            {"INVALID FILE OR MISSING BIOS...", "文件无效或缺少 BIOS…"},
            {"MISSING MEGA-CD BIOS DETECTED, EMULATION WILL LIKELY NOT WORK...",
                    "未检测到 Mega-CD BIOS，模拟可能无法正常运行…"},
            {"UNIBIOS DOESNT SUPPORT THE M68K ASM CORE\nCYCLONE ASM CORE DISABLED",
                    "UniBIOS 不支持 M68K 汇编内核\n已禁用 Cyclone 汇编内核"},
            {"ROM IS CRYPTED, USE DECRYPTED ROM (CLONE)\nTO ENABLE CYCLONE ASM CORE (FASTER)",
                    "ROM 已加密，请使用解密版（克隆版）\n以启用更快的 Cyclone 汇编内核"},
            {"THIS GAME DOES NOT SUPPORT THE M68K ASM CORE\nCYCLONE ASM CORE DISABLED",
                    "此游戏不支持 M68K 汇编内核\n已禁用 Cyclone 汇编内核"},
            {"THIS GAME IS NOT SUPPORTED BY FBNEO...", "FBNeo 不支持此游戏…"},
            {"DRIVER INIT FAILED", "驱动初始化失败"},
            {"TIPS: PRESS START BUTTON 2 SECONDS FOR DIAG MENU...", "提示：按住 START 键 2 秒进入诊断菜单…"},
            {"TIPS: PRESS COIN BUTTON 2 SECONDS TO RESET CURRENT GAME...", "提示：按住投币键 2 秒重置当前游戏…"}
    };

    /*
     * 设置项取值表：只影响界面显示，config.cfg 中仍保存原始英文取值。
     */
    const Entry kOptionValues[] = {
            {"OFF", "关"},
            {"ON", "开"},
            {"ALL", "全部"},
            {"AUTO", "自动"},
            {"ASPECT", "保持比例"},
            {"INTEGER", "整数倍"},
            {"NONE", "不缩放"},
            {"FIT", "适应屏幕"},
            {"FULL", "拉伸铺满"},
            {"POINT", "点采样"},
            {"LINEAR", "线性过滤"},
            {"FLIP", "翻转"},
            {"CAB MODE", "街机模式"},
            {"16/9", "16:9"},
            {"4/3", "4:3"}
    };

    std::string lookup(const Entry *table, std::size_t count, const std::string &key) {
        for (std::size_t i = 0; i < count; i++) {
            if (key == table[i].key) {
                return table[i].text;
            }
        }
        return key;
    }
}

namespace pemu {

    std::string tr(const std::string &key) {
        return lookup(kStaticText, sizeof(kStaticText) / sizeof(kStaticText[0]), key);
    }

    std::string trValue(const std::string &value) {
        return lookup(kOptionValues, sizeof(kOptionValues) / sizeof(kOptionValues[0]), value);
    }
}

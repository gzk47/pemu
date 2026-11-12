[![linux-release](https://github.com/Cpasjuste/pemu/actions/workflows/linux-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/linux-release.yml)
[![linux-dev](https://github.com/Cpasjuste/pemu/actions/workflows/linux-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/linux-dev.yml)  
[![windows-release](https://github.com/Cpasjuste/pemu/actions/workflows/windows-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/windows-release.yml)
[![windows-dev](https://github.com/Cpasjuste/pemu/actions/workflows/windows-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/windows-dev.yml)  
[![switch-release](https://github.com/Cpasjuste/pemu/actions/workflows/switch-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/switch-release.yml)
[![switch-dev](https://github.com/Cpasjuste/pemu/actions/workflows/switch-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/switch-dev.yml)  
[![3ds-release](https://github.com/Cpasjuste/pemu/actions/workflows/3ds-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/3ds-release.yml)
[![3ds-dev](https://github.com/Cpasjuste/pemu/actions/workflows/3ds-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/3ds-dev.yml)  
[![ps4-release](https://github.com/Cpasjuste/pemu/actions/workflows/ps4-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/ps4-release.yml)
[![ps4-dev](https://github.com/Cpasjuste/pemu/actions/workflows/ps4-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/ps4-dev.yml)  
[![vita-release](https://github.com/Cpasjuste/pemu/actions/workflows/vita-release.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/vita-release.yml)
[![vita-dev](https://github.com/Cpasjuste/pemu/actions/workflows/vita-dev.yml/badge.svg)](https://github.com/Cpasjuste/pemu/actions/workflows/vita-dev.yml)

# pEMU 中文版（Nintendo Switch）

这是基于 [Cpasjuste/pemu](https://github.com/Cpasjuste/pemu) 修改的 Nintendo Switch 前端中文版，包含五个模拟器核心：**pNES、pGEN、pSNES、pGBA、pFBneo**。

本版在原版英文版的基础上，只针对界面与交互做了改进：

1.UI 界面中文化

2.修复简介页首行空白的问题

3.优化游戏列表排序：数字、英文、拼音首字母

4.修复首次使用时无法扫描 ROM 路径的问题

5.新增ROM列表缓存，加快启动速度

6.新增游戏列表长名称跑马灯，简介页支持 L/R 翻页

上游的构建方式与核心逻辑没有改动，安装方式与原版一致。

<br />

***

## 本版新增功能

### 1. UI 界面中文化

菜单标题、设置项名称与选项取值（如"开 / 关""全部""自动""保持比例"）、底部帮助栏、状态栏提示、即时存档菜单、收藏确认弹窗、ROM 信息面板的字段标签（系统 / 开发商 / 发行商 / 日期 / 类型 / 玩家人数 / 评分 / 克隆自 / 文件）、扫描进度，以及各核心启动时的错误与警告弹窗，都已改为中文显示。

好处很直接：不用再对着英文菜单猜每个选项的意思，第一次用也能顺利把模拟器配置好。

使用时注意两点：

- **配置文件里的键名与取值仍是英文**。中文只是"显示层"，写回 `config.cfg` 的依然是原版英文值，所以旧的配置文件可以继续用，升级不会被重置，界面语言也不影响任何功能判断。
- **游戏名、系统名、发行商、类型等来自 `gamelist.xml` 的数据不会被翻译**，它们原样显示你 gamelist 里的内容。这也是为什么你可以直接放一份中文 gamelist 来让列表显示中文名。

### 2. 修复简介页首行空白的问题

此前打开游戏详情页时，中文简介上方会多出一整行空白，看起来像排版错位。现在中文简介从简介框的第一行开始正常显示，不再有顶部空行。

英文简介的排版与原来完全一致，不受影响。

### 3. 优化游戏列表排序：数字、英文、拼音首字母

列表不再按文件名的字符编码顺序排列，而是按显示名分成三段排序：

1. **数字开头**的游戏排在最前，按数值大小排（例如 `1943` 会排在 `1942` 之后，而不是按字符逐位比较）。
2. **英文字母开头**的游戏排在中间，按字母顺序排列，不区分大小写。
3. **中文名**的游戏排在最后，按**拼音首字母**分类排列（例如"超""赤""成"这类 C 开头的中文名会排在一起），同首字母内再按原有顺序排列。

好处是中文 gamelist 不再散落在列表末尾，找游戏时按拼音首字母就能快速定位，中英文混合的列表也更整齐。

使用时注意：

- 名字开头的空格、标点以及常见的全角标点会被忽略，按第一个有效字符归组，所以 `《超级马里奥》` 会按"超"来排。
- 极少数生僻字不在拼音表覆盖范围内，这些条目会排在中文组末尾，仍可正常显示和使用。
- 多音字按单字最常用的读音取首字母，个别游戏可能与你习惯的读法不同。
- 如果开启了"显示 ROM 文件名"选项，列表仍按原版方式以文件名排序，拼音排序只作用于显示名。

### 4. 修复首次使用时无法扫描 ROM 路径的问题

原版在首次使用、还没有生成配置文件的情况下，可能无法把默认的 ROM 目录写进配置，导致启动后列表是空的、看起来"没有扫描到 ROM"。

现在首次启动会正确创建 ROM 路径配置并开始扫描，把 ROM 放进对应目录后即可直接看到游戏列表，不需要先手动改配置。

如果列表仍然为空，优先检查 ROM 是否放进了对应核心的目录、文件扩展名是否被该核心支持。

### 5. 新增 ROM 列表缓存，加快启动速度

首次启动时会正常扫描全部 ROM 并生成一份列表缓存；之后再启动会优先读取缓存，跳过重复扫描，启动明显更快，ROM 越多差别越明显。

什么时候会自动重建缓存（不需要你手动处理）：

- `gamelist.xml` 的**大小或修改时间**发生变化；
- ROM 目录内的**文件数量或文件名**发生变化（增删、改名都会触发）。

也就是说，你新增游戏、删除游戏或更新 gamelist 后，下次启动会自动重扫并刷新缓存，不会出现"列表还是旧的"的情况；缓存不可用或写入失败时会自动回退为完整扫描，不会中断启动、也不会弹窗打扰。

小提示：上游原英文版每次启动软件都会扫描全部ROM，我们只在首次启动扫描全部ROM，之后直接读取缓存快速启动。

### 6. 新增游戏列表长名称跑马灯，简介页支持 L/R 翻页

- **长名称跑马灯**：列表里名字超出显示宽度的游戏，不再只能看到被截断的部分，名称会以滚动（跑马灯）方式完整展示，长中文名和长英文名都能看清全名。
- **简介页 L/R 翻页**：简介内容较长、一屏放不下时，可以用 **L / R 键翻页**查看后续内容，不必再忍受简介被裁掉。

这两项都是纯界面改进，不影响游戏运行与存档。

***

## 安装与使用提示

- 把五个 `.nro` 分别放到 Switch 对应目录（例如 `sdmc:/switch/pfbneo/`、`sdmc:/switch/pnes/` 等，与原版一致），ROM 放进各核心对应的 ROM 目录。
- `gamelist.xml` 请使用 **UTF-8** 保存，这样中文游戏名与中文简介才能正常显示。
- 本版所有配置项名称与取值仍与原版一致，升级时可直接沿用旧配置；主题（皮肤）也继续沿用原版目录约定。

***

## 与上游的关系及致谢

本仓库基于 [Cpasjuste/pemu](https://github.com/Cpasjuste/pemu) 修改，属于面向 Nintendo Switch 的中文本地化分支。上游的构建方式（devkitPro / Makefile）与五个模拟器核心均未改动，本版的改动仅限界面显示与交互层。

原项目及其所有贡献者提供了完整的模拟器前端框架，本版只是在其之上做了中文化与使用体验上的补充，在此致谢。

许可方面沿用上游：本项目同样以 **GNU General Public License v3.0** 发布，详见仓库根目录的 `LICENSE` 文件。

## pEMU: Portable Emulator

**<ins>Building</ins>**

- [Linux](https://github.com/Cpasjuste/pemu/blob/master/.github/workflows/linux-release.yml)
- [Windows](https://github.com/Cpasjuste/pemu/blob/master/.github/workflows/windows-release.yml)
- [Nintendo Switch](https://github.com/Cpasjuste/pemu/blob/master/.github/workflows/switch-release.yml)
- [Sony PS4](https://github.com/Cpasjuste/pemu/blob/master/.github/workflows/ps4-release.yml)
- [Sony PS VITA](https://github.com/Cpasjuste/pemu/blob/master/.github/workflows/vita-release.yml)

**<ins>Gamelists</ins>**

- "pEMU" emulators use some "gamelist.xml" files to manage your rom files and medias (EmulationStation/Recalbox format).
  Since version 7.0, the emulator will look inside your roms folders for a "gamelist.xml" file (which can be made
  with ["Skraper"](https://www.skraper.net/) for example).
  Please note that only the "\<image>" and "\<video>" media tags are supported, you should
  configure "["Skraper"](https://www.skraper.net/)" accordingly.

**<ins>Roms</ins>**

- You can add as many roms paths as you want in the emulators configuration file ("config.cfg"), and here is the format:
    ```
    NAME       SYSTEM    SYSTEM_ID     PATH
    ARCADE = ( "Arcade", "0x12341234", "/roms/fbneo/arcade/" );
    ```
  [Except for pfbneo](https://github.com/Cpasjuste/pemu/tree/master/src/cores/pfbneo), "NAME", "SYSTEM" and "SYSTEM_ID" can be anything (but a unique system id is required per path, for example "0x1", "0x2", ...).

**<ins>Skins</ins>**

- Default skins are [embedded (romfs)](https://github.com/Cpasjuste/pemu/tree/master/data/common/romfs/skins/default) in
  the application.
  You can overwrite parts of the default theme by creating a folder named "default" in the "skins" directory of the data
  folder, and adding
  a ["config.cfg.override"](https://github.com/Cpasjuste/pemu/tree/master/pgen/data/common/romfs/skins/default/config.cfg.override) (
  and/or
  a ["config.cfg.override.43"](https://github.com/Cpasjuste/pemu/tree/master/data/common/romfs/skins/default/config.cfg.override.43)
  for 4/3 screens) file.
  You can also create a new directory with a new name to create a new theme,
  the [default](https://github.com/Cpasjuste/pemu/tree/master/data/common/romfs/skins/default) one is a good start to
  look at.
  Please note that the theme code/api is subject to change without warning, do not loose too much time on this...

**<ins>Custom instructions</ins>**

- [pfbneo](https://github.com/Cpasjuste/pemu/tree/master/src/cores/pfbneo)
- [pgen](https://github.com/Cpasjuste/pemu/tree/master/src/cores/pgen)
- [pnes](https://github.com/Cpasjuste/pemu/tree/master/src/cores/pnes)
- [psnes](https://github.com/Cpasjuste/pemu/tree/master/src/cores/psnes)
- [pgba](https://github.com/Cpasjuste/pemu/tree/master/src/cores/pgba)

I use [JetBrains CLion](https://www.jetbrains.com/clion/) IDE for all my open sources projects, thanks to the support !

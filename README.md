# iidx-mods

本项目的模组由 **ChatGPT** 完成，如有疏漏，敬请谅解。欢迎通过 [Issue](https://github.com/HarroYuz/iidx-mods/issues) 反馈问题、复现步骤和改进建议。

本仓库包含 beatmania IIDX 的自选段位、练习和 no-fail 模组，沿用仓库已有的 [GNU GPL v3](LICENSE) 许可证。

**v0.1.1 修复**：no-fail 不再将中途退曲误判为通关；H/EXH 和段位内部血量低于 100、显示为 0% 时立即锁定失败，之后回血也不会恢复资格。v0.1.0 已撤下，请更新 `2dx-no-fail.dll`。

## 适配版本

**IIDX 34 ZINRAI**：

- LDJ2026091700-010

## 目录说明

| MOD 名称 | 用途 |
| --- | --- |
| [2dx-free-dan](2dx-free-dan/) | 逐首自选四曲组成段位，并扩展段位谱面选项与逐曲结算标记。 |
| [2dx-practice](2dx-practice/) | 提供歌曲倍速和 ALL SCRATCH，练习修改生效时禁止保存成绩。 |
| [2dx-no-fail](2dx-no-fail/) | 允许 H/EXH 和段位负血继续演奏，支持自动灯及主副屏负血显示。 |

[data_mods](data_mods/) 是**资源文件夹，不是模组**，存放自选段位与练习菜单的 PNG/XML，无需添加到 Hook。[licenses](licenses/) 存放第三方依赖的许可证。

## 安装与加载

适用于 Windows x64，通过 spice/spice2x 加载。

1. 在 [Releases](https://github.com/HarroYuz/iidx-mods/releases) 下载需要的 DLL，放到 **`bm2dx.dll` 同级目录**。三个模组可按需选择；建议共同使用同一版本的 DLL。
2. free-dan 和 practice 需要 `ifs_hook.dll` 及界面资源。从 Release 下载 `ifs_hook.dll`，放在同一目录；将 `data_mods.zip` 解压到游戏目录，得到 `data_mods/free_dan/` 和 `data_mods/practice/`。压缩包还附有许可证。no-fail 本身不需要额外图片。也可直接将仓库根目录的 `data_mods` 复制到游戏目录，保留其中的 `free_dan`、`practice` 目录结构。
3. 在 **spicecfg 的 Hook 配置**中添加 DLL，或在启动命令后追加 `-k xxx.dll`。使用界面资源时先加载 `ifs_hook.dll`；no-fail 放在 free-dan、practice 及自动灯插件之后。

共同加载示例，在原有启动参数后追加：

```bat
spice64.exe [原有参数] -k ifs_hook.dll -k 2dx-free-dan.dll -k 2dx-practice.dll -k 2dx-no-fail.dll
```

`[原有参数]` 仅为说明，请保留自己的服务器、显示等设置，不要原样输入方括号。使用自动灯插件时，将其 DLL 放在 `2dx-no-fail.dll` 前面。启动 `boot.bat` 时请**以管理员身份运行**。

更新前退出游戏，备份旧 DLL 和资源。界面资源未刷新时，退出游戏后备份并移走 `data_mods/_cache/graphic/0`、`1` 下对应的 `mselect_ifs` 或 `opt_select*_ifs` 缓存，让 ifs_hook 重建。不要同时加载旧整合版 `2dx-self-dan.dll` 与新的 free-dan/practice。

具体入口、操作、成绩保存规则见各子目录 README。

### 运行时依赖

附带的 `ifs_hook.dll` 来自 [mon/ifs_layeredfs v3.7](https://github.com/mon/ifs_layeredfs/releases/tag/v3.7) 的 64 位版本，用于加载界面资源。该 DLL 未作修改，仍适用上游许可证；源码与许可证见[依赖说明](THIRD_PARTY_NOTICES.md)。

## 从源码构建 DLL

推荐使用 **MinGW-w64 GCC 14 或更新版本、CMake 3.24+、Git、Python 3**，并将 `g++`、`gcc`、`mingw32-make`、`cmake`、`git`、`python` 加入 PATH。构建环境为 Windows x64；no-fail 使用 C++23，其余两个模组使用 C++17。

```powershell
git clone https://github.com/HarroYuz/iidx-mods.git
cd iidx-mods
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

产物位于 `build/bin/`：`2dx-free-dan.dll`、`2dx-practice.dll`、`2dx-no-fail.dll`。也可通过 `cmake --build build --target 2dx-no-fail` 等命令只构建指定模组。

首次配置会下载固定提交的 SafetyHook 和 Zydis（含 Zycore）；依赖及许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。构建不需要游戏或其他插件的二进制文件。生成的地址头文件已包含在仓库中；修改地址表后可运行各模组的 `tools/generate_addresses.py` 重新生成。

默认测试不需要游戏文件。`build/`、测试日志、游戏文件和调试转储不提交到 Git；根目录 `data_mods` 中的 PNG/XML 随源码一同提供。

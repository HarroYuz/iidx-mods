# 2dx-practice：歌曲倍速与 ALL SCRATCH

适配 **IIDX 34 ZINRAI / LDJ2026091700-010（20260917-010）**，Windows x64。支持版本从 34 开始，不支持 33 及更早版本；其他版本需另行适配。

## 功能与操作

在蓝色 **ADVANCED OPTION** 菜单末尾添加：

- **SONG RATE**：0.70～1.50，步进 0.05；黑键增加，白键减小。音频、谱面、选曲试听与 BGA 同步变速，音调随之变化。
- **ALL-SCR**：黑键开启，白键关闭；每侧每个时刻尽量分配一个转盘，和弦余音保留键盘，CN/BSS 按 LEGACY 处理。

非 1.00 倍速或 ALL-SCR 开启时，本曲成绩不保存，也不上传。恢复 1.00 且关闭 ALL-SCR 后，下一曲恢复原生保存资格；改变菜单设置不会提前解除前一曲的保护。检测到冲突的独立成绩导出插件时，练习设置会锁定。

## 安装与加载

1. 将 `2dx-practice.dll`、`ifs_hook.dll` 放到 `bm2dx.dll` 同级目录。
2. 将 Release 的 `data_mods.zip` 解压到游戏目录，或将仓库根目录的 `data_mods/practice/` 复制到游戏的同名目录，形成 `data_mods/practice/graphic/...`。Release 附带的 `ifs_hook.dll` 来自 [mon/ifs_layeredfs v3.7](https://github.com/mon/ifs_layeredfs/releases/tag/v3.7)。
3. 在 spicecfg 的 Hook 中先添加 `ifs_hook.dll`，再添加 `2dx-practice.dll`；或在原启动命令后追加：

```bat
-k ifs_hook.dll -k 2dx-practice.dll
```

以管理员身份运行 `boot.bat`。配置与日志分别位于游戏目录 `practice-mod/settings.ini`、`practice-mod/runtime.log`。若更新后菜单图片未刷新，退出游戏并备份移走对应 `opt_select*_ifs` 缓存，再重新启动。

## 段位与兼容性

可独立使用，也可搭配 `2dx-free-dan.dll`。自选段位四次选曲均可调整练习选项；确认后锁定，下一首选曲重新开放。普通段位也可设置练习选项。

课程中只要有一曲使用练习修改，整段课程就不保存成绩，即使之后恢复原速或关闭 ALL-SCR 也一样。与 free-dan 共同加载时，最终结算逐曲标记实际倍速与 ALL-SCR。no-fail 不会解除练习模组的成绩保存限制。

推荐顺序：

```bat
-k ifs_hook.dll -k 2dx-free-dan.dll -k 2dx-practice.dll -k 2dx-no-fail.dll
```

## 构建与验证

在仓库根目录按 [全局 README](../README.md#从源码构建-dll) 配置 CMake，再执行：

```powershell
cmake --build build --target 2dx-practice --parallel 4
```

输出 `build/bin/2dx-practice.dll`。也保留独立 `build.ps1 -Compiler g++ -Python python` 构建入口，输出本目录 `build/`。

测试覆盖速度边界、谱面处理、原生菜单与音频接口、请求分类、练习成绩保护，以及与 free-dan 的两种加载顺序和课程状态联动。`v0.1.0` 已由用户实机测试确认。

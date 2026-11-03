# 2dx-no-fail：负血继续演奏

适配 **IIDX 34 ZINRAI / LDJ2026091700-010（20260917-010）**，Windows x64。公开支持从 34 开始，不支持 33 及更早版本；其他版本需另行适配。

## 功能

- **HARD / EX HARD** 可降到负数并继续演奏。第一次触及 0 后，本曲该档永久 FAILED，回血转正不恢复资格。其他普通血条仍使用原生算法与下限，不会变成负数。
- 主屏使用原生数字字体显示负血绝对值，无负号，以红色区分；副屏显示带 `-` 的负血数字。主副屏结算折线沿用原比例，允许向下穿过图框。
- HARD 和段位的低血量补正，在零血、负血时继续生效。
- dead point 记录各档第一次触零的小节与音符位置，曲终或回血不会覆盖。

## 成绩与自动灯

固定 H/EXH 死亡后仍判 FAILED，但分数、miss count 可以照常保存和上传；其他模组的禁存条件仍然有效。

兼容本地 `2dx-gsm.dll` 和 FireNet 自动灯：并行计算 EXH、H、NORMAL、EASY、ASSISTED EASY，遵守开局档位与最低切换档，选择仍合格的最高档灯。较高档破血后，合格降档结果可保存；没有合格档时不上传。结算初始显示最优档，按 **VEFX** 切换血条视图，背景、血线与 dead point 同步变化；查看失败档不会改变保存档位。

副屏按各曲保存独立的显示历史，不用当前曲数据覆盖旧曲目。负值只写入本地有符号显示历史，不写入无符号上传 ghost 数据。

## 段位与自选段位

普通段位和 `2dx-free-dan.dll` 自选段位均允许负血打完当前曲。首次触零后当前曲永久失败，曲终结束整个段位；不再进入下一首，即使血量已回正。失败段位请求不上传，自选段位原有禁存规则继续生效。双人段位任一参与方触零也结束课程。

## 安装与加载

将 `2dx-no-fail.dll` 放到 **`bm2dx.dll` 同级目录**，在 spicecfg 的 Hook 中添加，或在原启动命令后追加：

```bat
-k 2dx-no-fail.dll
```

本模块不需要额外图片。与 free-dan、practice 或自动灯插件共同使用时，**no-fail 最后加载**，例如：

```bat
-k ifs_hook.dll -k 2dx-free-dan.dll -k 2dx-practice.dll -k firenet.dll -k 2dx-no-fail.dll
```

仅添加自己实际使用的插件；`firenet.dll` 与 `2dx-gsm.dll` 不包含在 Release 中。当前适配检查对应 GSM `ecf5d28b…`、FireNet `9de791ab…`（完整 SHA-256 见 `tools/audit.py`），其他插件版本需要另行验证。

以管理员身份运行 `boot.bat`。日志位于游戏目录 `no-fail-mod/runtime.log`。

## 构建与验证

需要完整仓库：no-fail 复用同级 practice 的地址、挂钩及请求策略头文件，联动测试使用 free-dan 的课程状态机。按 [全局 README](../README.md#从源码构建-dll) 配置 CMake，然后：

```powershell
cmake --build build --target 2dx-no-fail --parallel 4
```

输出 `build/bin/2dx-no-fail.dll`。SafetyHook、Zydis/Zycore 会从固定提交获取，**不需要本机 gsm-mod 源码目录**。

测试覆盖永久失败、自动降档、SP/DP 两侧、原生低血量补正、显示与保存分离、副屏历史与坐标，以及四首自选段位在负血或触零后回正时的曲终退出。`v0.1.0` 已经用户实机测试确认。

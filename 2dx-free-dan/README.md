# 2dx-free-dan：自选段位

适配 **IIDX 34 ZINRAI / LDJ2026091700-010（20260917-010）**，Windows x64。公开支持从 34 开始，不支持 33 及更早版本；其他版本需另行适配。

## 功能与操作

- STANDARD、PREMIUM FREE 的版本分类及 STEP UP 中新增「自选段位」文件夹。
- 进入后逐首选择四曲；后续曲目的等级不能低于上一曲。沿用原生段位扣血、曲间精确血量继承和结算，SP/DP 不可中途切换。
- 普通段位与自选段位开放 RANDOM、R-RANDOM、S-RANDOM、MIRROR、ASSIST，以及 DP 的左右独立随机、FLIP 等谱面选项；GAUGE 仍为段位血条。
- 结算逐曲列出实际生效的选项，影响保存资格的选项及 `[NO SAVE]` 以红色标注。

自选段位不写入官方段位成绩。普通段位使用扩展谱面选项或练习修改时，本次课程禁止保存；之后恢复设置也不会解除本课程保护。原生允许的 MIRROR 组合保留原有保存资格。

## 安装与加载

1. 将 `2dx-free-dan.dll`、`ifs_hook.dll` 放到 `bm2dx.dll` 同级目录。
2. 将 Release 的 `data_mods.zip` 解压到游戏目录，或将本目录 `assets-0917/` 内的 `graphic` 复制到游戏 `data_mods/self_dan/`，形成 `data_mods/self_dan/graphic/...`。Release 附带的 `ifs_hook.dll` 来自 [mon/ifs_layeredfs v3.7](https://github.com/mon/ifs_layeredfs/releases/tag/v3.7)。
3. 在 spicecfg 的 Hook 中先添加 `ifs_hook.dll`，再添加 `2dx-free-dan.dll`；或在原启动命令后追加：

```bat
-k ifs_hook.dll -k 2dx-free-dan.dll
```

以管理员身份运行 `boot.bat`。不要同时加载旧 `2dx-self-dan.dll`。更新入口图片后若仍显示旧图，退出游戏并备份移走对应的 `mselect_ifs` 缓存，让 ifs_hook 重建。

## 与其他模组共同使用

`2dx-practice.dll` 可在四次选曲期间分别设置倍速、ALL-SCR；确认曲目后锁定，下一首选曲再开放。结算显示各曲实际设置，课程保存保护不会提前解除。

搭配 `2dx-no-fail.dll` 时，段位可负血继续到当前曲结束；首次触零后本曲永久 FAILED，回血转正也不能恢复通过。曲终进入段位结算，不再选下一曲。负血失败结果保留，不影响自选段位原有禁存规则。

推荐顺序：

```bat
-k ifs_hook.dll -k 2dx-free-dan.dll -k 2dx-practice.dll -k 2dx-no-fail.dll
```

日志位于游戏目录 `self-dan-mod/logs/`。可选插件间通过导出函数交换状态，无强制相互导入依赖。

## 构建与验证

在仓库根目录按 [全局 README](../README.md#从源码构建-dll) 配置 CMake，再执行：

```powershell
cmake --build build --target 2dx-free-dan --parallel 4
```

输出 `build/bin/2dx-free-dan.dll`。也保留独立 `build.ps1 -Compiler g++ -Python python` 构建入口，输出本目录 `build/`。

自动测试覆盖四曲选择、血量继承、各曲失败结束、SP/DP、段位选项和保存保护；no-fail 联动测试使用真实原生结算行验证负血及触零后回血结果。`v0.1.0` 已经用户实机测试确认。

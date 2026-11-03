# 界面资源

`data_mods` 是**资源文件夹，不是模组**，不需要在 spicecfg 的 Hook 中添加，也不能用 `-k` 加载。这里的 PNG/XML 供 `ifs_hook.dll` 加载，用于显示自选段位和练习菜单。

## 适配版本

**IIDX 34 ZINRAI**：

- LDJ2026091700-010

## 资源与使用

| 子目录 | 用途 |
| --- | --- |
| `free_dan/` | 自选段位文件夹、标题及入口图标。 |
| `practice/` | SONG RATE、ALL-SCR 菜单图片及纹理布局。 |

将本文件夹复制到 `bm2dx.dll` 所在目录，保留 `data_mods/free_dan/graphic/...` 和 `data_mods/practice/graphic/...` 的结构。只使用其中一个模组时，可只复制对应的资源子目录。

将 Release 中的 `ifs_hook.dll` 放到 `bm2dx.dll` 同级目录，并在对应模组之前加载。完整步骤见[安装说明](https://github.com/HarroYuz/iidx-mods#安装与加载)。

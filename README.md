# 暴力摩托 30 周年纪念版｜原生分辨率自动适配与高清重绘补丁

**中文** | [English](README.en.md)

适用于 Steam《暴力摩托：30 周年纪念版》（build **26105967**）。补丁默认读取当前游戏窗口的实际绘制区域大小，按对应分辨率重新渲染比赛场景；支持不同分辨率与屏幕比例。**英文原版**及已安装独立中文汉化补丁的版本使用同一个安装包。

**汉化补丁请移步：** [RoadRash30th-Chinese](https://github.com/OBLPlife/RoadRash30th-Chinese)

## 实机截图

**高清比赛实机画面**

![高清比赛实机画面](screenshots/gameplay-wide.png)

| 游戏模式菜单 | 休息室菜单 |
| :---: | :---: |
| ![游戏模式菜单](screenshots/menu-game-mode.png) | ![休息室菜单](screenshots/menu-restroom.png) |

**游戏内分辨率设置（3440 × 1440）**

| 分辨率列表 | 已选择的分辨率 |
| :---: | :---: |
| ![游戏内分辨率选择列表](screenshots/resolution-list.png) | ![3440 × 1440 分辨率设置](screenshots/resolution-selected.png) |

## 功能

- **自动检测游戏分辨率**：渲染时读取当前游戏窗口的客户区宽高；在自动模式下，以该尺寸建立绘制缓冲区。窗口尺寸变化时会重新计算相应渲染尺寸和映射。
- **按目标分辨率重新绘制**：基于游戏原有的场景几何和纹理，在目标分辨率下重新光栅化赛道、道路及场景物体，而非把原始 640×480 比赛画面简单放大。
- **自动适配屏幕比例与视野**：根据实际宽高比计算场景投影范围、逻辑摄像机宽度和裁剪区域；适用于常见的 4:3、16:9、21:9 等比例，不采用固定宽高比白名单。
- **原生与自选渲染分辨率**：默认选择“自动（原生）”，按当前窗口尺寸渲染；也可以在“画面与声音 → 比赛分辨率”中选择其他内部渲染高度，宽度按当前比例计算，设置写入 `RoadRashHD.ini`。
- **菜单与文字显示适配**：原始菜单美术等比例居中，剩余区域采用镜像、柔化方式延展；界面与文字覆盖层按目标尺寸映射。
- **场景渲染问题修复**：优化远景、道路边缘及画面衔接，减少闪烁、远景缺失和接缝跳变。

渲染模块接管游戏原有的 640×480 高画质绘制路径，再生成目标尺寸的输出画面。

## 安装

1. 退出游戏。
2. 从 [Releases](https://github.com/OBLPlife/RoadRash30th-HD/releases) 下载 `RoadRash_HD_Setup.exe`，放到游戏根目录（与 `RoadRash95.exe` 同一目录）。
3. 双击安装器，完成后从 Steam 启动游戏。
4. 默认自动按游戏窗口的当前分辨率绘制，无需额外设置。如需调整内部渲染分辨率，可进入 **画面与声音 → 比赛分辨率**（英文版 **Graphics and Sound → Race Resolution**）选择。

**同时使用中文补丁时：先安装中文补丁，再安装高清补丁。**

## 配置与恢复

可在 `RoadRashHD.ini` 中设置：

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` 为默认自动模式：跟随当前游戏窗口的客户区尺寸（全屏时为游戏的全屏绘制区域），按对应宽高比进行原生重绘；正数指定内部渲染高度，宽度按窗口比例计算。最终显示画面仍适配当前窗口尺寸。

安装时会自动备份原有程序和高清模块。需要恢复时，退出游戏，在游戏目录的 PowerShell 中运行：

```powershell
.\RoadRash_HD_Setup.exe /restore
```

## 从源码构建

需要 Windows、Python 3.10+、Visual Studio C++ 桌面开发工具及 Windows SDK，以及 .NET Framework 4.x 编译器。

```powershell
python build.py
```

安装包输出至 `dist/RoadRash_HD_Setup.exe`。如需编译测试程序：

```powershell
python build.py --tests
```

核心渲染代码位于 `src/`，安装器代码位于 `Installer.cs` 和 `RendererPatch.cs`，测试代码位于 `tests/`。

## 许可证

本项目采用 [PolyForm Noncommercial License 1.0.0](LICENSE)，允许符合许可证的非商业使用、修改和分发；未经授权不得用于商业用途或收费倒卖。

# 暴力摩托 30 周年纪念版｜高清与宽屏修复补丁

**中文** | [English](README.en.md)

适用于 Steam《暴力摩托：30 周年纪念版》（build **26105967**）。支持**英文原版**和已安装独立中文汉化补丁的版本，使用同一个安装包。

**汉化补丁请移步：** [RoadRash-Chinese](https://github.com/OBLPlife/RoadRash-Chinese)

## 功能

- **原生高清渲染**：以实际分辨率绘制比赛场景、道路和纹理物体。
- **宽屏适配**：根据屏幕比例扩展视野，支持宽屏和超宽屏，保持物体比例。
- **游戏内分辨率选择**：可选择原生分辨率或其他渲染分辨率，设置自动保存。
- **菜单显示优化**：原画等比居中，背景随屏幕比例延展。
- **画面修复**：改善闪烁、远景道路缺失及接缝跳变问题。

## 安装

1. 退出游戏。
2. 从 [Releases](https://github.com/OBLPlife/RoadRash-HD/releases) 下载 `RoadRash_HD_Setup.exe`，放到游戏根目录（与 `RoadRash95.exe` 同一目录）。
3. 双击安装器，完成后从 Steam 启动游戏。
4. 进入 **画面与声音 → 比赛分辨率**，选择分辨率；英文版菜单为 **Graphics and Sound → Race Resolution**。更改后重新启动游戏生效。

**同时使用中文补丁时：先安装中文补丁，再安装高清补丁。**

## 配置与恢复

可在 `RoadRashHD.ini` 中设置：

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` 表示按当前窗口或全屏尺寸进行原生渲染；正数可指定内部渲染高度。

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

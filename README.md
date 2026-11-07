[中文](README.md) | [English](README.en.md)

# 暴力摩托：30周年纪念版｜原生分辨率自动适配与高清重绘补丁

适用于 Steam《暴力摩托：30周年纪念版》**build 26105967**。自动检测当前游戏窗口分辨率，并按实际尺寸重新绘制比赛场景，不是简单拉伸原版画面。支持英文原版及已安装独立汉化补丁的版本。

**汉化补丁请移步：** [RoadRash30th-Chinese](https://github.com/OBLPlife/RoadRash30th-Chinese)

## 安装与还原

**安装补丁**

1. 退出游戏，从 [Releases 发布页面](https://github.com/OBLPlife/RoadRash30th-HD/releases/latest) 下载 `RoadRash_HD_Setup.exe`。
2. 把 EXE 放到游戏根目录，与 `RoadRash95.exe` 放在一起。
3. **双击 EXE → 点击「安装」**。完成后从 Steam 启动游戏。

**默认自动适配分辨率，无需额外设置。** 需要调整内部渲染分辨率时，可在游戏内进入「画面与声音 → 比赛分辨率」（英文版：Graphics and Sound → Race Resolution）选择。

**还原原版**

退出游戏，再次双击同一个 EXE，点击 **「还原」**，即可恢复安装前的游戏程序和高清模块，无需使用 PowerShell 命令。

安装器自动备份需要修改的文件；不修改游戏存档或汉化文本。若同时使用汉化补丁，请 **先安装汉化、再安装高清补丁；还原时顺序相反**。

## 实机截图

**高清比赛画面**

![高清比赛实机画面](screenshots/gameplay-wide.png)

| 游戏模式菜单 | 休息室菜单 |
| :---: | :---: |
| ![游戏模式菜单](screenshots/menu-game-mode.png) | ![休息室菜单](screenshots/menu-restroom.png) |

**游戏内分辨率设置（3440 × 1440）**

| 分辨率列表 | 选中的分辨率 |
| :---: | :---: |
| ![分辨率选择列表](screenshots/resolution-list.png) | ![3440 × 1440 分辨率设置](screenshots/resolution-selected.png) |

## 功能

- **自动检测分辨率**：读取当前游戏窗口尺寸，默认按该尺寸绘制，并随窗口尺寸变化重新计算映射。
- **原生分辨率重绘**：在目标分辨率下重新绘制赛道、道路、场景几何与纹理物体，而不是单纯放大 640×480 画面。
- **自动适配画面比例**：根据宽高比调整视野和裁剪，兼容常见 4:3、16:9、21:9 等比例。
- **游戏内分辨率选择**：支持自动原生分辨率或较低的内部渲染分辨率，并保存设置。
- **菜单与场景修复**：菜单原画等比居中、背景自然延展；改善远景细节、画面闪烁和道路接缝问题。

## 配置（可选）

默认配置位于 `RoadRashHD.ini`，无需手动编辑：

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` 表示自动跟随游戏窗口尺寸；正数指定内部渲染高度，宽度按当前窗口比例计算。安装器会保留已有的分辨率设置。

## 从源码构建

需要 Windows、Python 3.10+、Visual Studio C++ 桌面开发工具与 Windows SDK，以及 .NET Framework 4.x 编译器。

```powershell
python build.py
```

安装器生成在 `dist/RoadRash_HD_Setup.exe`。如需编译并运行相关测试：

```powershell
python build.py --tests
python tests/test_installer.py --original-dir "D:\Games\RoadRashOriginal"
```

核心代码位于 `src/`，安装及恢复代码位于 `Installer.cs`、`PatchWindow.cs` 和 `RendererPatch.cs`；测试代码位于 `tests/`。构建过程不需要打包游戏本体。

## 许可证

本项目采用 [PolyForm Noncommercial License 1.0.0](LICENSE)，允许非商业使用、修改和分发；**禁止未经授权的商业使用及收费倒卖**。

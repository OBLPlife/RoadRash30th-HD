# 暴力摩托：30 周年纪念版 高清、分辨率与渲染补丁

独立安装包，包含当前 v4 高清绘制模块。适用于 **Steam build 26105967** 的英文原版、单独发布的纯汉化版及其对应 v4 高清版。

**只有一个安装 EXE，自动识别原版或汉化版，无需分别下载。** 原版继续使用原有语言，汉化版保留已有中文。

## 包含内容

- 原生高分辨率绘制道路、场景多边形及纹理物体，按窗口实际像素生成画面。
- 根据实际宽高比扩展比赛视野，支持普通宽屏、超宽屏、方形及竖屏，物体保持比例。
- 游戏内分辨率选择框，可选原生或较低分辨率，选择写入配置并在下次启动生效。
- 菜单完整原图等比居中，两侧或上下使用原画纹理的柔化镜像延展，保留原有背景切换。
- 跳过原版呈现流程的边缘刷黑及帧间背景擦除，修复由这些操作造成的闪烁。
- 保留小数顶点坐标，修复远景细小路面因取整后顶点误合并而消失、接缝跳变的问题。
- 并行绘制、缓存与界面合成优化。游戏计时、物理和原版帧率限制保持不变。

静态原画和纹理仍使用游戏原素材。菜单延展不是补绘的新场景；可用分辨率还受硬件及游戏 32 位进程内存限制。

## 安装

1. 退出游戏。
2. 将 `RoadRash_HD_Setup.exe` 放到游戏根目录，与 `RoadRash95.exe` 放在一起。
3. 双击 EXE 打开窗口，点击“安装”，弹窗提示安装完成后，从 Steam 启动游戏。
4. 打开“画面与声音 → 比赛分辨率”，选择“自动（原生 …）”或具体分辨率。英文版对应 `Graphics and Sound → Race Resolution`。

用户只需要一个安装 EXE；安装器会自动写入 `RoadRashHD.dll`，并在没有配置文件时创建 `RoadRashHD.ini`，无需手动复制模块。默认按当前窗口或全屏尺寸绘制。

打开窗口不会自动修改游戏文件。小窗口中只有“安装”和“还原”两个按钮，执行结果用弹窗提示。

如果同时使用独立汉化包，**先安装汉化，再安装本包**。本包不写入 `TEXT/ENU.DLL`，不替换游戏译文，不修改存档；已存在的分辨率配置也会保留。原程序与已有高清模块保存在 `分辨率补丁_安装前备份`。

安装器使用 Windows .NET Framework 4.x；渲染 DLL 已静态链接 C++ 运行库。安装失败可查看同目录的 `RoadRash_HD_Setup.log`。提示没有写入权限时，以管理员身份运行安装 EXE。

## 配置与恢复

`RoadRashHD.ini`：

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` 表示跟随窗口原生尺寸；正数指定内部高度，宽度按窗口比例计算。游戏菜单中更改分辨率会自动保存。手动修改配置后重新启动游戏。

退出游戏，再次双击同一个 EXE，在窗口中点击“还原”。也保留命令行用法，在游戏目录的 PowerShell 中运行：

```powershell
.\RoadRash_HD_Setup.exe /restore
```

恢复安装前的游戏程序和已有高清模块；如果之前没有高清模块，就移除本次安装的 DLL。分辨率配置、备份、安装器和日志保留，汉化与存档不动。如果还要卸载汉化，先还原本包，再在汉化包窗口点击“还原”。

`/quiet` 静默安装，`/quiet /restore` 静默还原。

## 从源码构建

需要 Windows、Python 3.10+、.NET Framework 4.x 自带的 C# 编译器，以及 Visual Studio 的“使用 C++ 的桌面开发”工具和 Windows SDK。

```powershell
python build.py
```

自动通过 `vswhere` 查找 C++ 工具；也可以指定：

```powershell
python build.py --vcvarsall "C:\Path\To\VC\Auxiliary\Build\vcvarsall.bat"
```

输出：`dist/RoadRash_HD_Setup.exe`。构建不需要游戏文件、第三方 Python 包、网络或 API 密钥。安装器只内嵌本项目的 DLL、默认配置和许可证，运行时为受支持的游戏程序添加高清加载入口；不包含完整游戏程序、语言库或素材。

## 检查

```powershell
python build.py --tests
python tests/test_installer.py --original-dir "D:\Games\RoadRashOriginal"
.\build\hd_test.exe "D:\Games\RoadRashOriginal\RoadRash95.exe"
```

如果还有纯汉化版的两个文件，可增加 `--localized-dir "D:\Games\RoadRashChinese"` 检查两者兼容性。

安装器测试在临时目录检查窗口只有安装与还原两个按钮且打开不写入，以及命令行安装、重复运行、已有设置保留、恢复及失败回滚，并通过实际 DLL 进行离屏绘制。窗口预览输出为 `build/window-english.png` 等。渲染测试覆盖九种尺寸、顶点与贴图顺序、透明纹理、多批次、菜单延展和完整帧输出。测试不启动游戏、不操作游戏界面；离屏检查不能替代实际游戏观感及帧率验收。

## 源码与许可

- `src/`：原生高清渲染、菜单资源、顶点处理和光栅化代码。
- `RendererPatch.cs`：保留原程序内容，添加 DPI 声明及高清加载入口。
- `Installer.cs`：安装、版本检查、备份和还原。
- `PatchWindow.cs`：只有安装与还原两个按钮的小窗口。
- `tests/`：安装器及离屏渲染检查。

源码公开，采用 **PolyForm Noncommercial License 1.0.0**。可按许可允许的非商业目的使用、修改和分发；许可范围之外的用途需另行授权。完整条款见 [LICENSE](LICENSE)。原版游戏、素材、商标与第三方组件不受本项目许可重新授权。

提交 GitHub 时提交本目录源码，`build/` 和 `dist/` 已忽略；安装 EXE 作为 Release 附件单独上传即可。

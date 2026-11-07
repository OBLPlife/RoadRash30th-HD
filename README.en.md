[中文](README.md) | [English](README.en.md)

# Road Rash: 30th Anniversary Edition — Native Resolution & HD Rendering Patch

For the Steam edition of **Road Rash: 30th Anniversary Edition**, build **26105967**. Supports the English original and the separate Chinese localization.

**Chinese localization:** [RoadRash30th-Chinese](https://github.com/OBLPlife/RoadRash30th-Chinese)

## Install and Restore

1. Download `RoadRash_HD_Setup.exe` from [Releases](https://github.com/OBLPlife/RoadRash30th-HD/releases/latest) and place it in the game folder, next to `RoadRash95.exe`.
2. Double-click the EXE and select **Install**.
3. Launch the game through Steam. Resolution is detected automatically; adjust it under **Graphics and Sound → Race Resolution** if desired.

To restore, open the same EXE and select **Restore**.

When using the Chinese localization, **install the localization first and the HD patch second; restore in reverse order**. The installer backs up the original files.

## In-Game Screenshots

**High-resolution gameplay**

![High-resolution gameplay](screenshots/gameplay-wide.png)

| Game Mode Menu | Restroom Menu |
| :---: | :---: |
| ![Game Mode Menu](screenshots/menu-game-mode.png) | ![Restroom Menu](screenshots/menu-restroom.png) |

**In-game resolution settings (3440 × 1440)**

| Resolution List | Selected Resolution |
| :---: | :---: |
| ![Resolution list](screenshots/resolution-list.png) | ![3440 × 1440 resolution selected](screenshots/resolution-selected.png) |

## Features

- **Automatic resolution detection:** Reads the game-window dimensions and redraws race scenes at the target resolution.
- **Native HD rendering:** Re-rasterizes roads, scene geometry and textured objects.
- **Aspect-ratio adaptation:** Adjusts the camera, projection and clipping for standard, widescreen and ultrawide layouts.
- **In-game resolution selection:** Automatic native mode or selectable internal rendering resolution, with saved settings.
- **Menu and rendering improvements:** Centers original menu art, extends surrounding backgrounds, and improves flicker, distant scenery and road seams.

### v1.1.1 Animation Crash Fix

A missing-animation-metadata case caused crashes on the Peninsula track. **The original game's animation-frame selector also crashes with the same invalid input.** The update checks the animation resource type, skips invalid frames and adds regression tests.

## Configuration (Optional)

`RoadRashHD.ini`:

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` selects automatic native resolution. A positive value sets the internal rendering height, with width derived from the current aspect ratio.

## Build from Source

Requires Windows, Python 3.10+, Visual Studio C++ Desktop Development tools, Windows SDK and a .NET Framework 4.x compiler.

```powershell
python build.py
```

Build and run tests:

```powershell
python build.py --tests
python tests/test_installer.py --original-dir "D:\Games\RoadRashOriginal"
```

Rendering code is in `src/`; installation code is in `Installer.cs`, `PatchWindow.cs` and `RendererPatch.cs`; tests are in `tests/`.

## License

This project uses the [PolyForm Noncommercial License 1.0.0](LICENSE). Noncommercial use, modification and redistribution are permitted; **unauthorized commercial use and resale are prohibited**.

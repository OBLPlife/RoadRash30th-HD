# Road Rash: 30th Anniversary Edition — HD & Widescreen Patch

[中文](README.md) | **English**

An HD rendering and widescreen patch for **Road Rash: 30th Anniversary Edition** on Steam (build **26105967**). Works with both the **original English game** and the separately released [Chinese localization](https://github.com/OBLPlife/RoadRash30th-Chinese), using the same installer.

## In-Game Screenshots

**Widescreen racing gameplay**

![Widescreen gameplay](screenshots/gameplay-wide.png)

| Game Mode Menu | Restroom Menu |
| :---: | :---: |
| ![Game Mode Menu](screenshots/menu-game-mode.png) | ![Restroom Menu](screenshots/menu-restroom.png) |

**In-game resolution settings (3440 × 1440)**

| Resolution List | Selected Resolution |
| :---: | :---: |
| ![In-game resolution list](screenshots/resolution-list.png) | ![3440 × 1440 resolution selected](screenshots/resolution-selected.png) |

## Features

- **Native high-resolution rendering** for race scenes, roads and textured objects.
- **Widescreen support** with an expanded field of view and correct object proportions, including ultrawide displays.
- **In-game resolution selection**, with settings saved automatically.
- **Improved menus** that preserve the original artwork's proportions and extend the background to fit the display.
- **Rendering fixes** for flicker, missing distant road details and shifting seams.

## Installation

1. Exit the game.
2. Download `RoadRash_HD_Setup.exe` from [Releases](https://github.com/OBLPlife/RoadRash30th-HD/releases) and place it in the game folder, next to `RoadRash95.exe`.
3. Run the installer, then launch the game through Steam.
4. Open **Graphics and Sound → Race Resolution** to select a resolution. Restart the game to apply the change.

**If you also use the Chinese localization, install the localization patch first, then the HD patch.**

## Configuration and Restore

You can edit `RoadRashHD.ini`:

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` uses the current window or fullscreen size for native rendering. Set a positive value to specify an internal rendering height.

The installer backs up the existing executable and HD module. To restore the previous state, exit the game and run this command in PowerShell from the game folder:

```powershell
.\RoadRash_HD_Setup.exe /restore
```

## Building from Source

Requires Windows, Python 3.10+, Visual Studio C++ Desktop Development tools and the Windows SDK, plus the .NET Framework 4.x compiler.

```powershell
python build.py
```

The installer will be generated at `dist/RoadRash_HD_Setup.exe`. To build the test executables as well:

```powershell
python build.py --tests
```

Rendering code is in `src/`, installer code is in `Installer.cs` and `RendererPatch.cs`, and tests are in `tests/`.

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). Noncommercial use, modification and distribution are permitted under its terms. Commercial use or resale requires separate permission.

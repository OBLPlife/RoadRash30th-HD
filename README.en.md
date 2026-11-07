# Road Rash: 30th Anniversary Edition — Automatic Native Resolution & HD Rendering Patch

[中文](README.md) | **English**

For **Road Rash: 30th Anniversary Edition** on Steam (build **26105967**). The patch automatically detects the game's current client-area dimensions and redraws race scenes at the corresponding resolution, adapting to different screen sizes and aspect ratios. The same installer supports the **original English game** and the separately released [Chinese localization](https://github.com/OBLPlife/RoadRash30th-Chinese).

## In-Game Screenshots

**High-resolution gameplay**

![High-resolution gameplay](screenshots/gameplay-wide.png)

| Game Mode Menu | Restroom Menu |
| :---: | :---: |
| ![Game Mode Menu](screenshots/menu-game-mode.png) | ![Restroom Menu](screenshots/menu-restroom.png) |

**In-game resolution settings (3440 × 1440)**

| Resolution List | Selected Resolution |
| :---: | :---: |
| ![In-game resolution list](screenshots/resolution-list.png) | ![3440 × 1440 resolution selected](screenshots/resolution-selected.png) |

## Actual Features

- **Automatic resolution detection:** Reads the game's client-area width and height during rendering. In automatic mode, the rendering buffers and mappings follow the current window dimensions, including when they change.
- **Native-resolution scene redraw:** Rasterizes the original scene geometry and textures—including race tracks, roads and objects—at the target resolution, rather than simply enlarging the legacy 640×480 race image.
- **Aspect-ratio-aware camera and view:** Recalculates the logical camera width, projection and scene culling from the current width-to-height ratio, including standard 4:3, widescreen 16:9 and ultrawide 21:9, without a fixed aspect-ratio whitelist.
- **Automatic or selectable rendering resolution:** Defaults to “Automatic (native)” at the current game-window dimensions. The in-game **Graphics and Sound → Race Resolution** menu also offers alternative internal rendering heights, with width calculated from the current aspect ratio; choices are saved in `RoadRashHD.ini`.
- **Menu and text presentation:** Keeps original menu artwork centered at its correct proportions, extends surrounding space using mirrored/softened artwork, and maps interface and text overlays to the output size.
- **Scene-rendering corrections:** Improves distant scenery, road edges and render continuity, reducing flicker, missing distant details and shifting seams.

The renderer hooks the game's original 640×480 high-detail drawing path and produces the output at the target dimensions.

## Installation

1. Exit the game.
2. Download `RoadRash_HD_Setup.exe` from [Releases](https://github.com/OBLPlife/RoadRash30th-HD/releases) and place it in the game folder, next to `RoadRash95.exe`.
3. Run the installer, then launch the game through Steam.
4. Native-resolution detection is automatic by default. To choose a different internal rendering resolution, open **Graphics and Sound → Race Resolution**.

**If you also use the Chinese localization, install the localization patch first, then the HD patch.**

## Configuration and Restore

You can edit `RoadRashHD.ini`:

```ini
[Graphics]
Enabled=1
RenderHeight=0
```

`RenderHeight=0` is the default automatic mode: it follows the current game-window client area (or the game's fullscreen drawing area), redrawing at that resolution and aspect ratio. A positive value specifies an internal rendering height; width follows the window's aspect ratio, while the final image still fits the current window.

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

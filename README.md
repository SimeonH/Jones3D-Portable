# Jones3D Portable

![Indiana Jones and the Infernal Machine](Resources/images/rmbanr.png)

Jones3D Portable is a complete, open source reimplementation of the **Jones3D engine**, the engine of *Indiana Jones and the Infernal Machine* (1999, *Indiana Jones und der Turm von Babel*). Its goal is to faithfully replicate the original game experience — the same gameplay, menus, dialogs, save games and help — while running natively on modern systems and on more than one platform.

The project builds on the decompilation work of [OpenJones3D](https://github.com/smlu/OpenJones3D). Every function of the original `Indy3D.exe` v1.0 has been reconstructed in C (2828 of 2828), so the game runs entirely on the reimplemented engine, with no code from the original executable.

This project does ***not*** include any game assets. You need a copy of the game, from [Steam](https://store.steampowered.com/app/904540), [GOG](https://www.gog.com/en/game/indiana_jones_and_the_infernal_machine) or the original CD-ROMs.

## Features

- **Multiple platforms** — runs natively on Windows and Linux (64-bit and 32-bit). Save games use the original file format and can be shared between platforms.
- **Original dialog boxes, recreated with wxWidgets** — every game dialog is rebuilt with native widgets, following the original layouts.
- **Vulkan and OpenGL ES 2.0 renderers** — modern rendering backends that replace DirectDraw / Direct3D 6, while drawing the same image as the original renderer. On Linux both are built in and selectable in the display settings.
- **In-game help** — the game's original WinHelp file opens in [Rust HLP Viewer](https://github.com/NullMagic2/Rust-HLP-Viewer), as current systems no longer ship a WinHelp viewer.
- **Game data installer** (Linux) — on first start, the game data is installed from a Steam installation, the GOG offline installer or the original CDs.
- **Original launcher** (Linux) — the game starts with a recreation of the CD's launcher (Start New Game, Resume Play, Help, Sound and Display Options), with its pictures and sounds taken from the CD or the Steam version. It's skipped when the game is started with arguments or in developer mode (`Start Mode=2`).

| Platform | Renderer | Platform layer | Sound |
|---|---|---|---|
| Windows (32-bit) | DirectDraw / Direct3D 6 (original) or Vulkan | Win32 | DirectSound |
| Linux (x86_64, i686) | Vulkan or OpenGL ES 2.0 | SDL3 | OpenAL Soft |

## Faithful to the original

The aim is the game as it shipped, not a remake. The engine logic is a function-by-function reconstruction of the original binary; on top of it, only the parts that tie the game to Windows 98-era technology are replaced.

A few quality-of-life improvements are included where the original shows its age, without changing how the game plays:

- **High resolution and widescreen support** — the game runs at modern resolutions, including above the 2048 pixel limit of Direct3D 6, and the camera's field of view, HUD and inventory menu are corrected for widescreen aspect ratios instead of being stretched.
- **High-detail models** are enabled by default.
- **The intro movie** is shown in its original 4:3 aspect ratio.
- **Save games** are listed sorted in the save and load dialogs.
- **Bug fixes** for original engine bugs, such as looping sounds that kept playing out of hearing range and NPCs getting stuck when facing each other.

### Dialogs with wxWidgets

The original game builds its menus and options screens from Win32 dialog templates stored in `Indy3D.exe`: the load and save dialogs, display and sound settings, control configuration, the store, the game over screen and more. Jones3D Portable recreates all of them with [wxWidgets](https://www.wxwidgets.org/), so they look and behave like the originals while working on any platform wxWidgets supports (Win32 on Windows, GTK 3 on Linux).

- Layouts follow the original dialog templates, including texts taken from the game's own string tables, so localized versions show their own language.
- The save and load dialogs are a clone of the classic Explorer-style file dialog the game used, including the game's custom preview area with the save game thumbnail.
- Dialog icons are read directly from the original `Indy3D.exe` (on Linux it's read as a data file, without the Win32 resource API).
- Dialogs pause the game, scale with the game window and keep the original keyboard behavior (Enter confirms, Escape cancels).

### Vulkan and OpenGL ES 2.0 renderers

The original engine draws through DirectDraw and Direct3D 6, which current drivers only support through compatibility layers. The GPU renderers implement the engine's `std3D` / `stdDisplay` interfaces (`Libs/std/Gpu`) over a Vulkan 1.3 backend (`Libs/std/Vulkan`) and an OpenGL ES 2.0 backend (`Libs/std/GLES2`), so the rest of the engine renders exactly as before:

- Each backend is a display device: on Linux, Vulkan and OpenGL ES 2.0 are both listed in the display settings and the game switches between them without a restart. OpenGL ES 2.0 is for GPUs and drivers without Vulkan 1.3.
- Vulkan is an option of the Windows build (`-DJONES3D_RENDERER=Vulkan`); the original DirectDraw renderer remains the Windows default. OpenGL ES 2.0 needs the SDL3 platform layer, so it's Linux only.
- The rendered image is scaled to fit the window, so the display mode never has to change.
- The 2D surfaces the original code draws into (HUD, menus, movies) are kept in system memory and uploaded when needed, as the game expects to lock and write them directly.
- Uses [volk](https://github.com/zeux/volk) to load Vulkan at runtime and dynamic rendering, Vulkan shaders are compiled at build time and embedded. OpenGL ES is loaded at runtime through SDL.

### In-game help

The game's Help menu opens `Jones3D.hlp`, a WinHelp file. Jones3D Portable launches [Rust HLP Viewer](https://github.com/NullMagic2/Rust-HLP-Viewer) for it, which is built together with the game and installed next to the game executable. A different viewer can be set with the `Help Viewer` setting.

## Installing on Linux

### Arch Linux

`packaging/arch/PKGBUILD` builds the `jones3d-portable` package. It installs the game to `/usr/lib/jones3d-portable` (with the help viewer), the `jones3d-portable` command and a desktop menu entry:

```bash
cd packaging/arch
makepkg -si
```

Start the game from the menu or with `jones3d-portable`. On first start, the data installer opens.

### Other distributions

Build the game from source (see [Building on Linux](#building-on-linux)) and run `build-linux64/Jones3D/jones3d-portable`.

### Where the game keeps its files

| Path | Contents |
|---|---|
| `~/.Jones3D/Resource` | Game data (`*.GOB`, intro movies, `Indy3D.exe`) |
| `~/.Jones3D/SaveGames` | Save games |
| `~/.Jones3D/KeySets` | Control schemes |
| `~/.config/OpenJones3D/settings.cfg` | Settings (the Windows registry settings of the original) |

A different data folder can be set with the `Install Path` setting. If the game data is in the working directory, it's used from there.

## The data installer

On Linux, the game checks for its data files (`Jones3D.GOB`, `CD1.GOB` and `CD2.GOB`) before it starts. When they aren't found, the data installer opens and copies everything the game needs to `~/.Jones3D`. It offers three sources and preselects the one it finds:

**Steam** (or another installed copy)
- The game is found automatically in all Steam libraries listed in `libraryfolders.vdf`, including Flatpak and Snap installs of Steam. Any other installed copy of the game can be chosen as well.
- The game data, intro movies, control schemes, help file and the Steam release's loose COG scripts are copied. Saved games can optionally be copied too; existing saves are never overwritten.
- The license agreement isn't shown again, as it was accepted when the game was installed through Steam.

**GOG offline installer**
- The offline installer for Indiana Jones and the Infernal Machine can be downloaded from the GOG website and the .exe installer file selected with this tool.
- It's unpacked with [innoextract](https://constexpr.org/innoextract/), which needs to be installed. First, the English end user license agreement is extracted from the installer and shown; the game is only installed when it's accepted.
- The game data is then extracted straight into the install folder, with progress, and the temporary files are removed.

**Original CD-ROMs**
- Mounted game discs are found automatically (e.g. under `/run/media`), a disc or mounted disc image can also be chosen manually.
- The original license agreement (`install/license.txt` on disc 1) is shown and must be accepted.
- Disc 1 is copied, then you're asked to insert disc 2. File names are matched case-insensitively, as CD file systems often show them in lower case.

Files are copied to temporary files first, so canceling never leaves a broken data file behind, and the free disk space (about 900 MB) is checked up front. To run the installer again, for example to switch to another source, start the game with `-installdata` or use *Install Game Data* in the desktop menu entry.

## Running on Windows

The Windows build doesn't have a data installer yet; it runs from the folder of an installed copy of the game.

1. Build the project (see [Building on Windows](#building-on-windows)).
2. Copy `OpenJones3D.exe` (and `hlp-viewer.exe`, if built) to the game's `Resource` folder, next to the original `Indy3D.exe`.
3. Run `OpenJones3D.exe`.

`Indy3D.exe` (v1.0, sha256 `3fbaf8cd401b4af80967cbe42e3420fb803288b336ebbe72a9a01b6dfd661a53`) is only read as a data file for its icon and dialog resources; none of its code runs.

The build also produces `Jones3D.dll`, which runs the reimplemented engine inside the original `Indy3D.exe` (started with `Jones3D.exe`), as in the original OpenJones3D project.

## Building

### Building on Linux

The Linux build uses SDL3, the Vulkan renderer, OpenAL Soft and wxWidgets with GTK 3. wxWidgets, OpenAL Soft, the Vulkan headers and volk are downloaded and built by CMake.

- **64-bit** (default): requires clang, CMake, Ninja and the SDL3, Vulkan loader and GTK 3 development packages.
  ```bash
  cmake -S . -B build-linux64 -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  cmake --build build-linux64
  ```
- **32-bit**: uses the i686 toolchain file and requires the 32-bit (multilib) versions of the libraries above.
  ```bash
  cmake -S . -B build-linux -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-linux-i686.cmake
  cmake --build build-linux
  ```

The help viewer is built with cargo when a Rust toolchain is installed.

### Building on Windows

Requires Visual Studio 2022, CMake and the DirectX 6.1 SDK:

1. Copy the headers and libraries of the DirectX 6.1 SDK to `Libs\external\DirectX61c\include` and `Libs\external\DirectX61c\lib`.
2. Generate and build the project:
   ```bash
   cmake -A win32 -B build .
   cmake --build build --config Release
   ```

The Windows build can also be cross-compiled from Linux with clang-cl, see `toolchain-clang-cl-win32.cmake`.

### Build options

| Option | Default | Description |
|---|---|---|
| `JONES3D_RENDERER` | `DirectDraw` (Windows), `Vulkan` (Linux) | Renderer backend |
| `JONES3D_RENDERER_GLES2` | `ON` (Linux) | Also builds the OpenGL ES 2.0 renderer |
| `JONES3D_BUILD_STANDALONE` | `ON` | Builds the standalone `OpenJones3D.exe` (Windows) |
| `JONES3D_BUILD_HLP_VIEWER` | `ON` when cargo is found | Builds Rust HLP Viewer for the in-game help |
| `JONES3D_QOL_IMPROVEMENTS` | `ON` | Quality-of-life improvements: better error handling, logging and debugging features |
| `FETCHCONTENT_SOURCE_DIR_<NAME>` | | Use a local source tree for a downloaded dependency (`WXWIDGETS`, `OPENALSOFT`, `VULKANHEADERS`, `VOLK`) |

## Engine architecture

The Jones3D engine is an upgraded version of the *Sith* engine of *Star Wars Jedi Knight: Dark Forces II* and *Mysteries of the Sith*. It consists of the original engine modules, plus `j3dcore`, which is specific to this implementation.

```
              ┌──────────────────────┐
              │       Jones3D        │   game flow, HUD, inventory, dialogs
              └──────────────────────┘
                         ▲
              ┌──────────────────────┐
              │         sith         │   game logic, COG scripts, physics, AI, saves
              └──────────────────────┘
                         ▲
              ┌──────────────────────┐
              │        rdroid        │   rendering, 3D models, animation, math
              └──────────────────────┘
                         ▲
 ┌───────────┬───────────┬───────────┬───────────┐
 │   sound   │    std    │  wkernel  │  w32util  │   platform: display, input, files, sound, settings
 └───────────┴───────────┴───────────┴───────────┘
```

- `w32util` — settings, stored in the Windows registry (a settings file on Linux).
- `wkernel` — game window and its events (Win32, or SDL3 on Linux).
- `std` — LucasArts' standard library: general utilities (strings, file paths, math, GOB archives) and the hardware layer (display, 3D, input, networking). The GPU renderers and the SDL3 platform code live here.
- `sound` — sound playback (DirectSound, or OpenAL Soft on Linux).
- `rdroid` — RenderDroid, the lower level of the rendering system: loads models, animations and textures, and draws primitives.
- `sith` — the heart of the game: game logic, the COG script VM, game objects (*things*), physics, collision, camera, AI and save games.
- `Jones3D` — starts the game and manages game flow, HUD, inventory menu and dialogs (`Jones3D/Gui/wx`).

Functions and data structures follow the names found in the original engine's debug strings, assertions and related engines, prefixed by the source file they belong to, e.g. `stdConffile_Open`, `sithThing_AddSwapEntry`. See the [OpenJones3D](https://github.com/smlu/OpenJones3D) project for the reconstruction methodology.

## Credits

- **[OpenJones3D](https://github.com/smlu/OpenJones3D)** by smlu and contributors — the decompilation of the Jones3D engine this project is built on. Its reverse engineering, reconstructed engine code and methodology are the foundation of Jones3D Portable.
- **[Rust HLP Viewer](https://github.com/NullMagic2/Rust-HLP-Viewer)** by NullMagic2 (MIT) — the WinHelp viewer that shows the game's help file.
- **[OpenJKDF2](https://github.com/shinyquagsire23/OpenJKDF2)** — reimplementation of the Sith engine of Jedi Knight, a valuable reference for the shared engine code.
- **[ScummVM](https://www.scummvm.org/)** — the SMUSH Blocky16 video codec used for the intro movie is ported from ScummVM's Grim engine.
- **[wxWidgets](https://www.wxwidgets.org/)**, **[SDL](https://www.libsdl.org/)**, **[OpenAL Soft](https://openal-soft.org/)**, **[volk](https://github.com/zeux/volk)** and the **[Vulkan Headers](https://github.com/KhronosGroup/Vulkan-Headers)** — libraries the game is built with.
- **[innoextract](https://constexpr.org/innoextract/)** by Daniel Scharrer — unpacks the GOG installer for the data installer.

## License

Jones3D Portable is licensed under the [GNU Affero General Public License v3.0](LICENSE).

Indiana Jones® and the Infernal Machine™ is Copyright © Lucasfilm Ltd. LucasArts, the LucasArts logo, Indiana Jones and related properties are trademarks of Lucasfilm Ltd. and/or its affiliates. This project is not affiliated with or endorsed by Lucasfilm, LucasArts, Disney, Steam or GOG.

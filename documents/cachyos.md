# Native Linux / CachyOS build

This fork retains SDL3, Dear ImGui and the Vulkan renderer. No Qt or Wine runtime
is required by the emulator. On Plasma, SDL's XDG portal dialog uses KDE's native
folder picker (the KDE file dialog, rather than launching Dolphin).

## Build

On an up-to-date Arch/CachyOS system, install the compiler, desktop development
libraries, Vulkan loader and the Mesa RADV driver:

```sh
sudo pacman -S --needed base-devel git clang libc++ libc++abi cmake ninja python \
  pkgconf alsa-lib libpulse openal systemd util-linux-libs libevdev libx11 libxext libxcursor \
  libxi libxrandr libxfixes libxss libxtst wayland wayland-protocols libdecor libxkbcommon dbus \
  vulkan-headers vulkan-icd-loader vulkan-radeon vulkan-tools vulkan-validation-layers \
  xdg-desktop-portal xdg-desktop-portal-kde
```

Current systemd/libudev provides the udev API. Most project libraries, including SDL3,
are built from pinned submodules rather than system versions. The Linux build
script applies three small, idempotent compatibility fixes to ZArchive, Sirit
and OpenAL Soft for newer libc++ headers; these appear as local submodule edits. Do not install
SDL2 in place of the bundled SDL3. Keep your existing PipeWire audio setup.

From a Git checkout with this fork's changes:

```sh
SHADPS4_BUILD_JOBS=4 bash scripts/build-linux.sh
```

The script builds the application and tests in separate directories because
`ENABLE_TESTS=ON` replaces the application target. Its test selection matches
the project's CI, excluding `GcnTest` Vulkan execution tests. It also exercises
Linux startup in fresh processes. Override CC/CXX if your Clang executable has
a version suffix. Initial development used Clang 19 and libc++ 19.

## Run

```sh
./build/linux/app/shadps4
./build/linux/app/shadps4 --game /home/tony/games/game-folder
./build/linux/app/shadps4 --game "/home/tony/games/a game with spaces"
./build/linux/app/shadps4 --big-picture
```

With no arguments, the application opens its SDL3/Dear ImGui launcher, using the
existing Big Picture interface. Enter a game folder, executable, or game ID,
or use **Choose Game Folder** for the native desktop picker, then press **Play**.
A complete game folder contains `eboot.bin` and `sce_sys/param.sfo`. If the portal
is unavailable, enter an absolute path in the game field.

**Launch Settings** saves patch selection, automatic-patch handling, fullscreen
mode, FPS display, clean/global configuration, log appending, override root,
debugger/process waits and same-process launching. **Emulator Settings** retains
the existing graphics, audio, input and other settings. Library and add-on folder
actions are also available under Launch Settings and take effect immediately.
The command-line help is available inside the settings panel.

Play normally closes the launcher and starts a fresh emulator process with these
arguments. Advanced settings can select the existing same-process behavior.
`--big-picture` explicitly opens the launcher; `--help` displays CLI help and exits.
Direct `--game` launches continue to work without opening the GUI. Paths are passed
as individual arguments, so spaces and shell punctuation remain literal.

Launch options are saved in `launcher.json` in the emulator user directory.
The old remembered game is imported when no new game selection has been saved.

The remembered selection lives under `$XDG_DATA_HOME/shadPS4/`, or
`~/.local/share/shadPS4/`. Existing portable `user/` directories take precedence.
Release and user flag files beside the executable are read before emulator
static initialization, with precedence: inherited environment, user flags, release
flags. Values are literal `NAME=VALUE` text, never shell commands. A bundled
`defaults/` tree copies only missing files. Existing settings are preserved.

## RX 9060 XT and Mesa

Install current Mesa/RADV and the distribution's firmware updates. Check:

```sh
vulkaninfo --summary
```

The output should list your Radeon and Mesa RADV with Vulkan 1.3 or newer. The
emulator already renders through Vulkan and automatically prefers a compatible
discrete GPU, then larger device-local memory. No NVIDIA libraries are required;
16 GB is detected from Vulkan memory heaps. Leave GPU selection automatic unless
you need to choose between multiple discrete cards in settings. Do not bundle a
Vulkan driver or force an ICD path from another machine.

On KDE Wayland, the normal desktop login should provide a D-Bus session and the
KDE portal backend. If the picker fails, inspect the user services for
`xdg-desktop-portal` and `xdg-desktop-portal-kde`. For display troubleshooting,
`SDL_VIDEO_DRIVER=x11` can select XWayland; otherwise allow SDL to choose.

Native Linux DLSS is not implemented by this fork. FSR 4.1/Redstone is not added;
AMD's [FSR SDK 2.3.0 documentation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/60f4ea81909200d8542eca14dccb2628b763a9a3/readme.md)
lists the ML upscaler as 4.1.1, but explicitly lists Vulkan as unsupported. The
existing FSR 1 spatial upscaler (EASU/RCAS) is retained; it is not FSR 4.1.
The fork's direct camera mouse-aim hook and Windows-specific scheduling
optimizations also remain Windows-only; their native Linux equivalents are
outside this launch/Vulkan change. Controller input and the existing SDL input
paths are retained.
Hardware rendering, KDE dialogs and actual gameplay must be validated on the
host: a cloud build without a Radeon or graphical session cannot establish them.

For a Clang build using your installed GCC standard library instead of libc++, use:

```sh
SHADPS4_BUILD_ROOT="$PWD/build/linux-libstdcxx" CXXFLAGS="-stdlib=libstdc++" bash scripts/build-linux.sh
```

An existing test, `WindowsGuestRedZoneProtectionProfileTest.GlobalConfigCannotEnableProtection`,
fails because the settings loader accepts this Windows setting from global JSON.
The Linux startup tests pass. `HostOverride.ActiveExactHostMatchFromJsonFile` is
skipped by its existing test logic. The script reports CTest's exit status after
also running the independent Linux startup and CLI checks.

For the investigated Second Son loading crash and a targeted heap diagnostic, see
[Linux allocation investigation](Debugging/second-son-linux-allocation.md).

# Linux port validation

Validated in the cloud on Debian 13, x86-64, with Clang/libc++ 19.1.7,
CMake 3.31.6 and Ninja 1.12.1. This is not a CachyOS hardware certification.

## Passed

- Full native Release build (`ENABLE_TESTS=OFF`, C++23, `-stdlib=libc++ -fexperimental-library`).
- Repeat build reports no work to do.
- Executable `--help` exits successfully and advertises game-folder paths.
- An existing folder without `eboot.bin`, including spaces in its path, is rejected with exit 1.
- Native picker failure reports the direct `--game /absolute/path` fallback without hanging.
- SDL is built with D-Bus/portal support and both Wayland and X11 backends; dynamic library names
  refer to host runtime SONAMEs, not build-machine static archives.
- Five Linux startup tests: early flags, unavailable picker error handling, remembered Unicode/spaced
  paths, game-folder versus library-folder discovery, and portable/XDG selection precedence.
- Fresh-process integration checks for flags before static initialization, BOM/CRLF, literal values,
  inherited/user/release precedence, default copying, and preserving existing settings.
- Mesa Lavapipe Vulkan device enumeration (Vulkan 1.4.305, Mesa 25.0.7).
- Existing `GcnTest.add_f32`: shader translation and compute execution on Mesa Lavapipe, with the
  Khronos validation layer enabled. This software driver is used only in cloud checks, not forced
  or bundled in the host preview.

## Existing suite outcomes

The CI test selection (`ctest ... -E GcnTest`) runs 387 tests:

- 385 passed, including five Linux startup tests and eight launch-options tests.
- 1 failed: `WindowsGuestRedZoneProtectionProfileTest.GlobalConfigCannotEnableProtection`.
  The current settings loader accepts the Windows protection setting from global JSON, contrary
  to the existing test's expectation. Its implementation and assertions were not changed.
- 1 skipped by existing test logic: `HostOverride.ActiveExactHostMatchFromJsonFile`.

The remaining GPU instruction tests were not run. The representative Vulkan compute test above
was run separately. No real game files were available for a gameplay check.

## Host checks still needed

1. On CachyOS, `vulkaninfo --summary` should identify the RX 9060 XT and the RADV driver.
2. Launch with no arguments and confirm the launcher opens. Use **Choose Game Folder**
   to check KDE's native picker, or enter an absolute path. Press Play to launch.
3. Change Launch Settings, restart the launcher and check persistence and gameplay behavior.
   Direct `--game /home/tony/games/game-folder` remains supported.
4. Add a library using Launch Settings or Emulator Settings and check game cards.
5. Check real gameplay, Vulkan rendering, controller/input behavior and settings on your GPU.
   The existing direct mouse-camera hook and DLSS integrations remain Windows-only.

FSR 4.1/Redstone was investigated but not integrated: AMD's pinned SDK documentation referenced
in [the CachyOS guide](cachyos.md) states that Vulkan is unsupported. Existing FSR 1 is retained.

## Launcher GUI validation

- Rebuilt the native Release application with the updated SDL3/Dear ImGui launcher.
- Exercised the actual window under Xvfb with SDL software rendering: main screen,
  empty-game validation, launch settings, advanced options and the fixed Save footer.
- Typed game IDs and patch paths, toggled FPS, saved and reopened the launcher;
  both the selected game and settings were restored.
- Pressed Play and verified the child process received separate game, patch (including
  spaces), FPS and parent-wait arguments. A deliberately unavailable game ID exited
  through normal CLI resolution without reopening the GUI.
- Exercised same-process Play through the same normal game resolution path.
- Fixed the existing WASD-to-arrow key mapping that interfered with typing paths.
- Eight model tests cover flag generation, literal arguments, validation, CLI overrides,
  persistence and malformed settings. These and the five startup tests pass.

No game files are available here; successful gameplay and KDE's native portal still
require host testing. Xvfb verifies the launcher UI, not hardware rendering.

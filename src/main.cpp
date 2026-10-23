// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#include <CLI/CLI.hpp>
#include <SDL3/SDL_messagebox.h>

#include "common/arch.h"
#include "common/key_manager.h"
#include "common/logging/log.h"
#include "common/memory_patcher.h"
#include "common/path_util.h"
#include "common/release_mode.h"
#include "core/debugger.h"
#include "core/emulator_settings.h"
#include "core/emulator_state.h"
#include "core/file_sys/fs.h"
#include "core/ipc/ipc.h"
#include "core/user_settings.h"
#include "emulator.h"
#include "imgui/big_picture/big_picture.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

int main(int argc, char* argv[]) {
    // Portable startup can change cwd before main. Keep child launches independent
    // of the relative path the shell or desktop entry originally used.
#ifdef __linux__
    std::error_code executable_error;
    std::string executable_path =
        std::filesystem::read_symlink("/proc/self/exe", executable_error).string();
    char* executable_name = executable_error ? argv[0] : executable_path.data();
#else
    char* executable_name = argv[0];
#endif
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    // Release mode (common/release_mode.h): started by a double-click, the console is ours alone.
    if (Common::ReleaseMode::Active()) {
        Common::ReleaseMode::HideOwnConsole();
    }

#if defined(__APPLE__) && defined(ARCH_X86_64)
    // KosmicKrisp only supports Apple Silicon. Check that we are not running on an Intel Mac.
    int sysctl_ret = 0;
    size_t sysctl_size = sizeof(sysctl_ret);
    sysctlbyname("sysctl.proc_translated", &sysctl_ret, &sysctl_size, nullptr, 0);
    if (sysctl_ret != 1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "shadPS4",
                                 "shadPS4 only supports Apple Silicon Macs.", nullptr);
        std::cout << "shadPS4 only supports Apple Silicon Macs." << std::endl;
        return -1;
    }
#endif

    CLI::App app{"shadPS4 Emulator CLI"};

    // ---- CLI state ----
    std::optional<std::string> gamePath;
    std::vector<std::string> gameArgs;
    std::optional<std::filesystem::path> overrideRoot;
    std::optional<int> waitPid;
    bool waitForDebugger = false;

    std::optional<std::string> fullscreenStr;
    bool ignoreGamePatch = false;
    bool showFps = false;
    bool configClean = false;
    bool configGlobal = false;
#ifdef __linux__
    bool bigPicture = argc == 1;
#else
    bool bigPicture = false;
#endif
    bool sameProcess = false;

    std::optional<std::filesystem::path> addGameFolder;
    std::optional<std::filesystem::path> setAddonFolder;
    std::optional<std::string> patchFile;

    // ---- Options ----
    app.add_option("-g,--game", gamePath, "Game folder, executable path or ID");
    app.add_option("-p,--patch", patchFile, "Patch file to apply");
    app.add_flag("-i,--ignore-game-patch", ignoreGamePatch,
                 "Disable automatic loading of game patches");

    app.add_flag("-b,--big-picture", bigPicture, "Start in Big Picture Mode");
    app.add_flag("--same-process", sameProcess,
                 "Launch the game in the same process when using Big Picture Mode");

    // FULLSCREEN: behavior-identical
    app.add_option("-f,--fullscreen", fullscreenStr, "Fullscreen mode (true|false)");

    app.add_option("--override-root", overrideRoot)->check(CLI::ExistingDirectory);

    app.add_flag("--wait-for-debugger", waitForDebugger);
    app.add_option("--wait-for-pid", waitPid);

    app.add_flag("--show-fps", showFps);
    app.add_flag("--config-clean", configClean);
    app.add_flag("--config-global", configGlobal);
    app.add_flag("--log-append", Common::Log::g_should_append);

    app.add_option("--add-game-folder", addGameFolder)->check(CLI::ExistingDirectory);
    app.add_option("--set-addon-folder", setAddonFolder)->check(CLI::ExistingDirectory);

    // ---- Capture args after `--` verbatim ----
    app.allow_extras();
    app.parse_complete_callback([&]() {
        const auto& extras = app.remaining();
        if (!extras.empty()) {
            gameArgs = extras;
        }
    });

    // ---- No-args behavior ----
    // Linux opens the launcher. Other release builds retain their game picker.
    std::optional<std::filesystem::path> releaseGame;
    if (argc == 1 && !bigPicture && Common::ReleaseMode::Active()) {
        releaseGame = Common::ReleaseMode::GamePath();
        if (!releaseGame) {
            return 0; // picker cancelled
        }
        gamePath = Common::FS::PathToUTF8String(*releaseGame);
    } else if (argc == 1 && !bigPicture) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "shadPS4",
                                 "This is a CLI application. Please use the QTLauncher for a GUI:\n"
                                 "https://github.com/shadps4-emu/shadps4-qtlauncher/releases",
                                 nullptr);
        std::cout << app.help();
        return -1;
    }

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Initialize main log with default config
    Common::Log::Setup("shadps4.log");

    LOG_INFO(Debug, "Run: {}", std::span(argv, argc));
    if (Common::ReleaseMode::Active()) {
        LOG_INFO(Config,
                 "Release mode: {} variables from {} ({} already set by the environment or "
                 "shadps4-flags-user.txt), {} from shadps4-flags-user.txt, {} default files copied",
                 Common::ReleaseMode::FlagsApplied(), Common::ReleaseMode::FlagsFilePath(),
                 Common::ReleaseMode::FlagsOverridden(), Common::ReleaseMode::UserFlagsApplied(),
                 Common::ReleaseMode::DefaultsCopied());
    }

    IPC::Instance().Init();

    auto emu_state = std::make_shared<EmulatorState>();
    EmulatorState::SetInstance(emu_state);
    UserSettings.Load();

    // Initialize key manager
    auto key_manager = KeyManager::GetInstance();
    key_manager->LoadFromFile();

    // Load configurations
    std::shared_ptr<EmulatorSettingsImpl> emu_settings = std::make_shared<EmulatorSettingsImpl>();
    EmulatorSettingsImpl::SetInstance(emu_settings);
    emu_settings->Load();

    // Configure logger appropriately
    Common::Log::g_should_append |= EmulatorSettings.IsLogAppend();

    if (bigPicture) {
        Launcher::Options initial;
        initial.game = gamePath.value_or("");
        initial.patch = patchFile.value_or("");
        initial.override_root = overrideRoot ? overrideRoot->string() : "";
        initial.ignore_game_patch = ignoreGamePatch;
        initial.show_fps = showFps;
        initial.log_append = Common::Log::g_should_append;
        initial.wait_for_debugger = waitForDebugger;
        initial.wait_for_process = waitPid.has_value();
        initial.wait_pid = waitPid.value_or(0);
        initial.same_process = sameProcess;
        initial.config_mode = configGlobal ? 2 : configClean ? 1 : 0;
        if (fullscreenStr) {
            if (*fullscreenStr != "true" && *fullscreenStr != "false") {
                LOG_ERROR(Debug, "Invalid argument for --fullscreen (use true|false)");
                return 1;
            }
            initial.fullscreen = *fullscreenStr == "true" ? 1 : 2;
        }
        auto launch = BigPictureMode::Launch(initial, app.help());
        if (!launch)
            return 0;
        if (!launch->same_process) {
            auto* emulator = Common::Singleton<Core::Emulator>::Instance();
            emulator->executableName = executable_name;
            emulator->Relaunch(launch->Arguments());
        }
        // Same-process launches use the same flag application and path resolution
        // below as normal CLI launches, rather than bypassing them in the GUI.
        gamePath = launch->game;
        patchFile = launch->patch.empty() ? std::nullopt : std::optional{launch->patch};
        overrideRoot = launch->override_root.empty()
                           ? std::nullopt
                           : std::optional{std::filesystem::path{launch->override_root}};
        ignoreGamePatch = launch->ignore_game_patch;
        showFps = launch->show_fps;
        waitForDebugger = launch->wait_for_debugger;
        waitPid = launch->wait_for_process ? std::optional{launch->wait_pid} : std::nullopt;
        fullscreenStr =
            launch->fullscreen == 0
                ? std::nullopt
                : std::optional<std::string>{launch->fullscreen == 1 ? "true" : "false"};
        configClean = launch->config_mode == 1;
        configGlobal = launch->config_mode == 2;
        Common::Log::g_should_append = launch->log_append || EmulatorSettings.IsLogAppend();
    }

    if (waitPid)
        Core::Debugger::WaitForPid(*waitPid);

    // ---- Utility commands ----
    if (addGameFolder) {
        EmulatorSettings.AddGameInstallDir(*addGameFolder);
        EmulatorSettings.Save();
        LOG_INFO(Config, "Game folder successfully saved.");
        return 0;
    }

    if (setAddonFolder) {
        EmulatorSettings.SetAddonInstallDir(*setAddonFolder);
        EmulatorSettings.Save();
        LOG_INFO(Config, "Addon folder successfully saved.");
        return 0;
    }

    if (!gamePath.has_value()) {
        if (!gameArgs.empty()) {
            gamePath = gameArgs.front();
            gameArgs.erase(gameArgs.begin());
        } else {
            LOG_ERROR(Debug, "Please provide a game path or ID.");
            return 1;
        }
    }
    if (!gameArgs.empty()) {
        if (gameArgs.front() == "--") {
            gameArgs.erase(gameArgs.begin());
        } else {
            LOG_ERROR(Debug, "unhandled flags");
            return 1;
        }
    }

    // ---- Apply flags ----
    if (patchFile)
        MemoryPatcher::patch_file = *patchFile;

    if (ignoreGamePatch)
        Core::FileSys::MntPoints::ignore_game_patches = true;

    if (fullscreenStr) {
        if (*fullscreenStr == "true") {
            EmulatorSettings.SetFullScreen(true);
        } else if (*fullscreenStr == "false") {
            EmulatorSettings.SetFullScreen(false);
        } else {
            LOG_ERROR(Debug, "Invalid argument for --fullscreen (use true|false)");
            return 1;
        }
    }

    if (showFps)
        EmulatorSettings.SetShowFpsCounter(true);

    if (configClean)
        EmulatorSettings.SetConfigMode(ConfigMode::Clean);

    if (configGlobal)
        EmulatorSettings.SetConfigMode(ConfigMode::Global);

    // ---- Resolve game path or ID ----
    // The picked path keeps its wide characters (a narrow string is read in the ANSI code page).
    std::filesystem::path ebootPath = releaseGame ? *releaseGame : std::filesystem::path(*gamePath);
    if (std::filesystem::is_directory(ebootPath)) {
        ebootPath /= "eboot.bin";
        if (!std::filesystem::is_regular_file(ebootPath)) {
            LOG_ERROR(Debug, "Game folder has no eboot.bin: {}", *gamePath);
            return 1;
        }
    }
    const auto archive_component_exists = [](const std::filesystem::path& p) -> bool {
        std::filesystem::path accum;
        for (const auto& comp : p) {
            accum /= comp;
            if (comp.extension() == ".zar") {
                return std::filesystem::is_regular_file(accum);
            }
        }
        return false;
    };
    if (!std::filesystem::exists(ebootPath) && !archive_component_exists(ebootPath)) {
        bool found = false;
        constexpr int maxDepth = 5;
        for (const auto& installDir : EmulatorSettings.GetGameInstallDirs()) {
            if (auto foundPath = Common::FS::FindGameByID(installDir, *gamePath, maxDepth)) {
                ebootPath = *foundPath;
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_ERROR(Debug, "Game ID or file path not found: {}", *gamePath);
            return 1;
        }
    }

    auto* emulator = Common::Singleton<Core::Emulator>::Instance();
    emulator->executableName = executable_name;
    emulator->waitForDebuggerBeforeRun = waitForDebugger;
    emulator->Run(ebootPath, gameArgs, overrideRoot);

    return 0;
}

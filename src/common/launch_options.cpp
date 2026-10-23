// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/launch_options.h"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace Launcher {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Options, game, patch, override_root,
                                                ignore_game_patch, show_fps, log_append,
                                                wait_for_debugger, wait_for_process, wait_pid,
                                                fullscreen, config_mode, same_process)

std::vector<std::string> Options::Arguments() const {
    std::vector<std::string> args{"--game", game};
    if (!patch.empty())
        args.insert(args.end(), {"--patch", patch});
    if (!override_root.empty())
        args.insert(args.end(), {"--override-root", override_root});
    if (ignore_game_patch)
        args.push_back("--ignore-game-patch");
    if (show_fps)
        args.push_back("--show-fps");
    if (log_append)
        args.push_back("--log-append");
    if (wait_for_debugger)
        args.push_back("--wait-for-debugger");
    if (wait_for_process)
        args.insert(args.end(), {"--wait-for-pid", std::to_string(wait_pid)});
    if (fullscreen != 0)
        args.insert(args.end(), {"--fullscreen", fullscreen == 1 ? "true" : "false"});
    if (config_mode == 1)
        args.push_back("--config-clean");
    if (config_mode == 2)
        args.push_back("--config-global");
    return args;
}

std::string Options::Validate() const {
    if (game.empty())
        return "Select a game folder, executable, or game ID first.";
    if (fullscreen < 0 || fullscreen > 2 || config_mode < 0 || config_mode > 2)
        return "Invalid launch settings. Choose a display and configuration mode again.";
    if (wait_for_process && wait_pid <= 0)
        return "The process ID must be greater than zero.";
    std::error_code ec;
    if (!override_root.empty() && !std::filesystem::is_directory(override_root, ec))
        return "The override root must be an existing directory.";
    const std::filesystem::path path{game};
    if (std::filesystem::is_directory(path, ec)) {
        if (!std::filesystem::is_regular_file(path / "eboot.bin", ec) ||
            !std::filesystem::is_regular_file(path / "sce_sys/param.sfo", ec))
            return "Choose the complete game folder containing eboot.bin and sce_sys/param.sfo.";
    } else if ((path.is_absolute() || game.find('/') != std::string::npos ||
                game.find('\\') != std::string::npos) &&
               !std::filesystem::is_regular_file(path, ec)) {
        return "The game path does not exist. Check the folder or executable path.";
    }
    return {};
}

bool Options::Save(const std::filesystem::path& file, std::string& error) const {
    try {
        if (!file.parent_path().empty())
            std::filesystem::create_directories(file.parent_path());
        // Write beside the destination and rename, so a partial write cannot
        // replace the previous configuration. This file contains no shell code.
        auto temporary = file;
        temporary += ".tmp";
        std::ofstream out{temporary};
        if (!(out << json(*this).dump(2) << '\n'))
            throw std::runtime_error("Could not write launch settings");
        out.close();
        if (!out)
            throw std::runtime_error("Could not flush launch settings");
#ifdef _WIN32
        // Windows rename cannot replace an existing destination.
        std::filesystem::copy_file(temporary, file,
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(temporary);
#else
        std::filesystem::rename(temporary, file);
#endif
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

Options Options::Load(const std::filesystem::path& file, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        return {};
    try {
        std::ifstream input{file};
        auto options = json::parse(input).get<Options>();
        if (options.fullscreen < 0 || options.fullscreen > 2 || options.config_mode < 0 ||
            options.config_mode > 2 || options.wait_pid < 0)
            throw std::runtime_error("Invalid saved launch setting");
        return options;
    } catch (const std::exception& ex) {
        error = "Could not load launch settings: " + std::string{ex.what()};
        return {};
    }
}

void Options::ApplyOverrides(const Options& cli) {
    if (!cli.game.empty())
        game = cli.game;
    if (!cli.patch.empty())
        patch = cli.patch;
    if (!cli.override_root.empty())
        override_root = cli.override_root;
    ignore_game_patch |= cli.ignore_game_patch;
    show_fps |= cli.show_fps;
    log_append |= cli.log_append;
    wait_for_debugger |= cli.wait_for_debugger;
    same_process |= cli.same_process;
    if (cli.wait_for_process) {
        wait_for_process = true;
        wait_pid = cli.wait_pid;
    }
    if (cli.fullscreen)
        fullscreen = cli.fullscreen;
    if (cli.config_mode)
        config_mode = cli.config_mode;
}
} // namespace Launcher

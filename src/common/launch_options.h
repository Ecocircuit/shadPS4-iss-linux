// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Launcher {
struct Options {
    std::string game;
    std::string patch;
    std::string override_root;
    bool ignore_game_patch = false;
    bool show_fps = false;
    bool log_append = false;
    bool wait_for_debugger = false;
    bool wait_for_process = false;
    int wait_pid = 0;
    int fullscreen = 0;  // 0: configured default, 1: fullscreen, 2: windowed
    int config_mode = 0; // 0: configured default, 1: clean, 2: global
    bool same_process = false;

    // Returns argv elements, never a shell command. Launcher/utility switches
    // are deliberately not forwarded to avoid reopening the GUI or exiting early.
    std::vector<std::string> Arguments() const;
    std::string Validate() const;
    bool Save(const std::filesystem::path& file, std::string& error) const;
    static Options Load(const std::filesystem::path& file, std::string& error);
    void ApplyOverrides(const Options& command_line);
};
} // namespace Launcher

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef __linux__

#include "common/native_folder_dialog.h"
#include "common/release_mode.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <SDL3/SDL.h>
#include <unistd.h>

namespace Common::ReleaseMode {
namespace {
namespace fs = std::filesystem;

std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == text.npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

struct FlagsFile {
    fs::path dir;
    int applied = 0;
    int overridden = 0;
    int user_applied = 0;
    int defaults_copied = 0;

    static void Apply(const fs::path& path, int& applied, int& overridden) {
        // No shell evaluation: values, including spaces and '$', are literal.
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) {
            return;
        }
        char* buffer = nullptr;
        size_t capacity = 0;
        bool first = true;
        ssize_t size;
        while ((size = getline(&buffer, &capacity, file)) >= 0) {
            std::string_view line{buffer, static_cast<size_t>(size)};
            if (first && line.starts_with("\xEF\xBB\xBF")) {
                line.remove_prefix(3);
            }
            first = false;
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            const auto eq = line.find('=');
            if (eq == line.npos) {
                continue;
            }
            const std::string name{Trim(line.substr(0, eq))};
            const std::string value{Trim(line.substr(eq + 1))};
            if (name.empty()) {
                continue;
            }
            if (std::getenv(name.c_str())) {
                ++overridden;
            } else if (setenv(name.c_str(), value.c_str(), 0) == 0) {
                ++applied;
            }
        }
        std::free(buffer);
        std::fclose(file);
    }

    FlagsFile() {
        std::error_code ec;
        dir = fs::read_symlink("/proc/self/exe", ec).parent_path();
        if (ec || dir.empty()) {
            return;
        }
        int skipped = 0;
        Apply(dir / "shadps4-flags-user.txt", user_applied, skipped);
        Apply(dir / "shadps4-flags.txt", applied, overridden);
        const auto defaults = dir / "defaults";
        if (fs::is_directory(defaults, ec)) {
            for (auto it = fs::recursive_directory_iterator(defaults, ec);
                 !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                std::error_code file_ec;
                if (!it->is_regular_file(file_ec)) {
                    continue;
                }
                const auto target = dir / fs::relative(it->path(), defaults, file_ec);
                if (file_ec || fs::exists(target, file_ec)) {
                    continue;
                }
                fs::create_directories(target.parent_path(), file_ec);
                if (fs::copy_file(it->path(), target, fs::copy_options::skip_existing, file_ec)) {
                    ++defaults_copied;
                }
            }
        }
        // Keep CLI paths relative to the caller's directory. Only explicitly
        // portable bundles (a user/ directory beside the binary) change cwd.
        if (fs::is_directory(dir / "user", ec)) {
            fs::current_path(dir, ec);
        }
    }
};

// Flags must precede namespace-scope readers in the emulator. This constructor
// uses C file I/O, not iostreams, whose initialization has not happened yet.
FlagsFile flags __attribute__((init_priority(200)));

fs::path DataDir() {
    std::error_code ec;
    auto dir = fs::current_path() / "user";
    if (fs::is_directory(dir, ec)) {
        return dir;
    }
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg) {
        dir = fs::path{xdg} / "shadPS4";
    } else if (home && *home) {
        dir = fs::path{home} / ".local/share/shadPS4";
    }
    fs::create_directories(dir, ec);
    return dir;
}

std::optional<fs::path> GameExecutable(fs::path path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        path /= "eboot.bin";
    }
    if (fs::is_regular_file(path, ec) &&
        fs::is_regular_file(path.parent_path() / "sce_sys/param.sfo", ec)) {
        return fs::absolute(path, ec);
    }
    return {};
}
} // namespace

bool Active() {
    return true;
}
int FlagsApplied() {
    return flags.applied;
}
int FlagsOverridden() {
    return flags.overridden;
}
int UserFlagsApplied() {
    return flags.user_applied;
}
int DefaultsCopied() {
    return flags.defaults_copied;
}
std::string FlagsFilePath() {
    return (flags.dir / "shadps4-flags.txt").string();
}
void HideOwnConsole() {}
void ShowOutOfMemoryNotice(unsigned long long bytes) {
    std::fprintf(stderr,
                 "shadPS4: allocation of %llu MiB failed. Check available RAM, "
                 "VRAM and swap; see the emulator log.\n",
                 bytes >> 20);
}

std::optional<fs::path> GamePath() {
    const auto saved = DataDir() / "shadps4-game.txt";
    std::ifstream input{saved};
    std::string line;
    if (std::getline(input, line)) {
        if (auto game = GameExecutable(fs::path{Trim(line)})) {
            return game;
        }
    }
    for (;;) {
        auto choice = Common::PickNativeFolder();
        if (!choice.error.empty()) {
            std::cerr << "Folder picker: " << choice.error
                      << "\nYou can run: shadps4 --game /absolute/path/to/game-folder\n";
            if (!isatty(STDIN_FILENO)) {
                SDL_ShowSimpleMessageBox(
                    SDL_MESSAGEBOX_ERROR, "shadPS4",
                    "The desktop folder picker is unavailable. Install and enable your desktop's "
                    "XDG portal, or run shadps4 --game /absolute/path/to/game-folder.",
                    nullptr);
                return {};
            }
            std::cerr << "Game folder (empty to cancel): ";
            if (!std::getline(std::cin, line) || line.empty()) {
                return {};
            }
            choice.path = fs::path{line};
        }
        if (!choice.path) {
            return {}; // Cancellation is not an error and must not open another picker.
        }
        if (auto game = GameExecutable(*choice.path)) {
            std::ofstream out{saved};
            if (!(out << game->string() << '\n')) {
                std::cerr << "Could not remember game selection in " << saved << '\n';
            }
            return game;
        }
        const char* message = "Choose the complete game folder containing eboot.bin and "
                              "sce_sys/param.sfo.";
        std::cerr << message << '\n';
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Invalid game folder", message, nullptr);
    }
}
} // namespace Common::ReleaseMode
#endif

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <vector>

namespace Common {
inline bool IsGameFolder(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path / "eboot.bin", ec) &&
           std::filesystem::is_regular_file(path / "sce_sys/param.sfo", ec);
}

// Accept both a selected game itself and the traditional parent library folder.
inline std::vector<std::filesystem::path> FindGameFolders(const std::filesystem::path& location) {
    if (IsGameFolder(location)) {
        return {location};
    }
    std::vector<std::filesystem::path> games;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(location, ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (!name.ends_with("-UPDATE") && !name.ends_with("-patch") && IsGameFolder(it->path())) {
            games.push_back(it->path());
        }
    }
    return games;
}
} // namespace Common

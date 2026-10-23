// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct SDL_Window;

namespace Common {
struct FolderDialogResult {
    std::optional<std::filesystem::path> path;
    std::string error;
};

// SDL uses the XDG desktop portal on Linux (KDE's picker on Plasma), with its
// supported fallback when a portal is unavailable. Call only on the main thread.
FolderDialogResult PickNativeFolder(SDL_Window* parent = nullptr);
} // namespace Common

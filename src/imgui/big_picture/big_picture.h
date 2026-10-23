// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <vector>
#include <SDL3/SDL.h>
#include <imgui.h>

#include "common/types.h"
#include "common/launch_options.h"

namespace BigPictureMode {

struct IconInfo {
    ImTextureID textureId;
    std::filesystem::path ebootPath;
    std::string title;
    std::string serial;
    bool focusState;
};

std::optional<Launcher::Options> Launch(const Launcher::Options& commandLine,
                                      const std::string& commandLineHelp);
void GetGameIconInfo(std::vector<IconInfo>& icons);
SDL_Texture* LoadSdlTextureData(std::vector<u8> data);
SDL_Texture* LoadSdlTextureDataFromFile(std::filesystem::path filePath);

} // namespace BigPictureMode

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/native_folder_dialog.h"

#include <atomic>
#include <SDL3/SDL.h>

namespace Common {
FolderDialogResult PickNativeFolder(SDL_Window* parent) {
    struct Request {
        FolderDialogResult result;
        std::atomic<bool> done{false};
    } request;
    const bool own_video = !(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO);
    if (own_video && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return {{}, SDL_GetError()};
    }
    SDL_ShowOpenFolderDialog(
        [](void* userdata, const char* const* files, int) {
            auto& req = *static_cast<Request*>(userdata);
            if (!files) {
                req.result.error = SDL_GetError();
                if (req.result.error.empty()) {
                    req.result.error = "The desktop folder picker is unavailable.";
                }
            } else if (files[0]) {
                req.result.path = std::filesystem::path{files[0]};
            }
            req.done.store(true, std::memory_order_release);
        },
        &request, parent, nullptr, false);
    while (!request.done.load(std::memory_order_acquire)) {
        // Portal responses are dispatched through SDL's D-Bus event processing.
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    if (own_video) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    return std::move(request.result);
}
} // namespace Common

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include "common/game_folder.h"
#include "common/native_folder_dialog.h"
#include "common/release_mode.h"

namespace {
// Captures the value before main, just like the emulator's feature flags.
const std::string early_flag =
    std::getenv("SHADPS4_STARTUP_TEST") ? std::getenv("SHADPS4_STARTUP_TEST") : "";

TEST(LinuxStartup, FlagsAreAvailableBeforeMain) {
    const char* expected = std::getenv("SHADPS4_STARTUP_EXPECTED");
    if (expected) {
        EXPECT_EQ(early_flag, expected);
    }
    EXPECT_TRUE(Common::ReleaseMode::Active());
}

TEST(LinuxStartup, UnavailableNativePickerReportsError) {
    const char* video = SDL_GetHint(SDL_HINT_VIDEO_DRIVER);
    const char* dialog = SDL_GetHint(SDL_HINT_FILE_DIALOG_DRIVER);
    const std::optional<std::string> old_video =
        video ? std::optional<std::string>{video} : std::nullopt;
    const std::optional<std::string> old_dialog =
        dialog ? std::optional<std::string>{dialog} : std::nullopt;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_FILE_DIALOG_DRIVER, "unsupported-test-driver",
                            SDL_HINT_OVERRIDE);
    const auto choice = Common::PickNativeFolder();
    EXPECT_FALSE(choice.path);
    EXPECT_FALSE(choice.error.empty());
    EXPECT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0u);
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    SDL_ResetHint(SDL_HINT_FILE_DIALOG_DRIVER);
    if (old_video)
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, old_video->c_str());
    if (old_dialog)
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, old_dialog->c_str());
}

class LinuxSavedGame : public testing::Test {
protected:
    std::filesystem::path temp;
    std::filesystem::path old_cwd;
    std::string old_xdg;
    bool had_xdg = false;
    void SetUp() override {
        char name[] = "/tmp/shadps4-startup-XXXXXX";
        ASSERT_NE(mkdtemp(name), nullptr);
        temp = name;
        old_cwd = std::filesystem::current_path();
        if (const char* value = std::getenv("XDG_DATA_HOME")) {
            had_xdg = true;
            old_xdg = value;
        }
        setenv("XDG_DATA_HOME", (temp / "data").c_str(), 1);
        std::filesystem::current_path(temp);
        std::filesystem::create_directories(temp / "game with spaces/日本語/sce_sys");
        std::ofstream(temp / "game with spaces/日本語/eboot.bin") << "fixture";
        std::ofstream(temp / "game with spaces/日本語/sce_sys/param.sfo") << "fixture";
    }
    void TearDown() override {
        std::filesystem::current_path(old_cwd);
        if (had_xdg) {
            setenv("XDG_DATA_HOME", old_xdg.c_str(), 1);
        } else {
            unsetenv("XDG_DATA_HOME");
        }
        std::filesystem::remove_all(temp);
    }
};

TEST_F(LinuxSavedGame, LoadsAbsoluteFolderWithSpacesAndUnicode) {
    const auto game = temp / "game with spaces/日本語";
    std::filesystem::create_directories(temp / "data/shadPS4");
    std::ofstream(temp / "data/shadPS4/shadps4-game.txt") << game.string() << '\n';
    EXPECT_EQ(Common::ReleaseMode::GamePath(), game / "eboot.bin");
}

TEST_F(LinuxSavedGame, LibraryAcceptsGameItselfOrParentDirectory) {
    const auto game = temp / "game with spaces/日本語";
    EXPECT_EQ(Common::FindGameFolders(game), std::vector<std::filesystem::path>{game});
    EXPECT_EQ(Common::FindGameFolders(game.parent_path()),
              std::vector<std::filesystem::path>{game});
    std::filesystem::create_directories(temp / "incomplete");
    std::ofstream(temp / "incomplete/eboot.bin") << "fixture";
    EXPECT_FALSE(Common::IsGameFolder(temp / "incomplete"));
    EXPECT_TRUE(Common::FindGameFolders(temp / "missing").empty());
}

TEST_F(LinuxSavedGame, PortableSelectionTakesPrecedenceOverXdg) {
    const auto game = temp / "game with spaces/日本語/eboot.bin";
    std::filesystem::create_directories(temp / "user");
    std::ofstream(temp / "user/shadps4-game.txt") << game.string() << "\r\n";
    EXPECT_EQ(Common::ReleaseMode::GamePath(), game);
}
} // namespace

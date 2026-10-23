// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <unistd.h>
#include "common/launch_options.h"

namespace {
TEST(LaunchOptions, DefaultArgumentsStartGameWithoutReopeningLauncher) {
    Launcher::Options options;
    options.game = "CUSA00004";
    EXPECT_EQ(options.Arguments(), (std::vector<std::string>{"--game", "CUSA00004"}));
}

TEST(LaunchOptions, PathsAndSpecialCharactersRemainSingleLiteralArguments) {
    Launcher::Options options;
    options.game = "/home/tony/Games/Second Son 日本語";
    options.patch = "/tmp/my patch;$(false).xml";
    options.override_root = "/tmp/override root";
    EXPECT_EQ(options.Arguments(),
              (std::vector<std::string>{"--game", options.game, "--patch", options.patch,
                                        "--override-root", options.override_root}));
}

TEST(LaunchOptions, AllRuntimeFlagsAreForwardedAndCleanGlobalAreExclusive) {
    Launcher::Options options;
    options.game = "CUSA00004";
    options.ignore_game_patch = options.show_fps = options.log_append = true;
    options.wait_for_debugger = options.wait_for_process = options.same_process = true;
    options.wait_pid = 123;
    options.fullscreen = 2;
    options.config_mode = 1;
    EXPECT_EQ(options.Arguments(),
              (std::vector<std::string>{"--game", "CUSA00004", "--ignore-game-patch", "--show-fps",
                                        "--log-append", "--wait-for-debugger", "--wait-for-pid",
                                        "123", "--fullscreen", "false", "--config-clean"}));
    options.config_mode = 2;
    auto args = options.Arguments();
    EXPECT_EQ(args.back(), "--config-global");
    EXPECT_EQ(std::count(args.begin(), args.end(), "--config-clean"), 0);
    EXPECT_EQ(std::count(args.begin(), args.end(), "--same-process"), 0);
}

TEST(LaunchOptions, RejectsEmptyGameInvalidModesAndInvalidPid) {
    Launcher::Options options;
    EXPECT_FALSE(options.Validate().empty());
    options.game = "CUSA00004";
    EXPECT_TRUE(options.Validate().empty());
    options.wait_for_process = true;
    EXPECT_FALSE(options.Validate().empty());
    options.wait_pid = 123;
    EXPECT_TRUE(options.Validate().empty());
    options.fullscreen = 9;
    EXPECT_FALSE(options.Validate().empty());
}

TEST(LaunchOptions, ExplicitCliValuesOverrideSavedSettingsWithoutErasingOtherChoices) {
    Launcher::Options saved, cli;
    saved.game = "old";
    saved.patch = "keep.xml";
    saved.show_fps = true;
    saved.config_mode = 1;
    cli.game = "new";
    cli.fullscreen = 2;
    cli.config_mode = 2;
    saved.ApplyOverrides(cli);
    EXPECT_EQ(saved.game, "new");
    EXPECT_EQ(saved.patch, "keep.xml");
    EXPECT_TRUE(saved.show_fps);
    EXPECT_EQ(saved.fullscreen, 2);
    EXPECT_EQ(saved.config_mode, 2);
}

class LaunchSettingsFile : public testing::Test {
protected:
    std::filesystem::path root;
    void SetUp() override {
        char pattern[] = "/tmp/shadps4-launch-options-XXXXXX";
        ASSERT_NE(mkdtemp(pattern), nullptr);
        root = pattern;
    }
    void TearDown() override {
        std::filesystem::remove_all(root);
    }
};

TEST_F(LaunchSettingsFile, SavedOptionsSurviveReload) {
    Launcher::Options options;
    options.game = "/home/tony/Second Son 日本語";
    options.patch = "my patch.xml";
    options.override_root = root.string();
    options.ignore_game_patch = options.show_fps = options.log_append = true;
    options.wait_for_debugger = options.wait_for_process = options.same_process = true;
    options.wait_pid = 456;
    options.fullscreen = options.config_mode = 2;
    std::string error;
    const auto file = root / "new/launcher.json";
    ASSERT_TRUE(options.Save(file, error)) << error;
    auto loaded = Launcher::Options::Load(file, error);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(loaded.Arguments(), options.Arguments());
    EXPECT_TRUE(loaded.same_process);
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
}

TEST_F(LaunchSettingsFile, MissingOrMalformedSettingsDoNotCrashOrModifyFile) {
    std::string error;
    const auto file = root / "launcher.json";
    EXPECT_TRUE(Launcher::Options::Load(file, error).game.empty());
    EXPECT_TRUE(error.empty());
    std::ofstream(file) << "{ broken json";
    EXPECT_TRUE(Launcher::Options::Load(file, error).game.empty());
    EXPECT_FALSE(error.empty());
    std::ifstream input{file};
    std::string contents{std::istreambuf_iterator<char>{input}, {}};
    EXPECT_EQ(contents, "{ broken json");
}

TEST_F(LaunchSettingsFile, RejectsInvalidGameFolderAndAcceptsCompleteFolder) {
    Launcher::Options options;
    options.game = root.string();
    EXPECT_FALSE(options.Validate().empty());
    std::ofstream(root / "eboot.bin") << "fixture";
    EXPECT_FALSE(options.Validate().empty());
    std::filesystem::create_directory(root / "sce_sys");
    std::ofstream(root / "sce_sys/param.sfo") << "fixture";
    EXPECT_TRUE(options.Validate().empty());
    options.override_root = (root / "missing").string();
    EXPECT_FALSE(options.Validate().empty());
}
} // namespace

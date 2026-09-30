// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/display_server_utils.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND) && BUILDFLAG(SUPPORTS_OZONE_X11)

namespace ui {

using EnvVar = base::ScopedEnvironmentVariableOverride;

class SetOzonePlatformForLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(runtime_dir_.CreateUniqueTempDir());
    xdg_runtime_dir_.emplace("XDG_RUNTIME_DIR", runtime_dir_.GetPath().value());
  }

 protected:
  void CreateDefaultWaylandSocket() {
    ASSERT_TRUE(
        base::WriteFile(runtime_dir_.GetPath().Append("wayland-0"), ""));
  }

  std::string PickPlatform() {
    SetOzonePlatformForLinuxIfNeeded(command_line_);
    return command_line_.GetSwitchValueASCII(switches::kOzonePlatform);
  }

  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};

 private:
  base::ScopedTempDir runtime_dir_;
  EnvVar no_session_type_{"XDG_SESSION_TYPE"};
  EnvVar no_display_{"DISPLAY"};
  EnvVar no_wayland_display_{"WAYLAND_DISPLAY"};
  EnvVar no_wayland_socket_{"WAYLAND_SOCKET"};
  std::optional<EnvVar> xdg_runtime_dir_;
};

TEST_F(SetOzonePlatformForLinuxTest, KeepsExplicitPlatform) {
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  command_line_.AppendSwitchASCII(switches::kOzonePlatform, "x11");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, NoDisplayAndNoSessionTypePicksX11) {
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, NoDisplayFollowsSessionType) {
  EnvVar session_type("XDG_SESSION_TYPE", "wayland");
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, BothDisplaysFollowWaylandSessionType) {
  EnvVar session_type("XDG_SESSION_TYPE", "wayland");
  EnvVar display("DISPLAY", ":0");
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, BothDisplaysFollowX11SessionType) {
  EnvVar session_type("XDG_SESSION_TYPE", "x11");
  EnvVar display("DISPLAY", ":0");
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, BothDisplaysWithoutSessionTypePickX11) {
  EnvVar display("DISPLAY", ":0");
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, BothDisplaysInTtySessionPickX11) {
  EnvVar session_type("XDG_SESSION_TYPE", "tty");
  EnvVar display("DISPLAY", ":0");
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, OnlyWaylandDisplayPicksWayland) {
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, OnlyWaylandSocketPicksWayland) {
  EnvVar wayland_socket("WAYLAND_SOCKET", "5");
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, OnlyDefaultWaylandSocketPicksWayland) {
  CreateDefaultWaylandSocket();
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, OnlyDisplayPicksX11InWaylandSession) {
  EnvVar session_type("XDG_SESSION_TYPE", "wayland");
  EnvVar display("DISPLAY", ":0");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest,
       OnlyDisplaySwitchPicksX11InWaylandSession) {
  EnvVar session_type("XDG_SESSION_TYPE", "wayland");
  command_line_.AppendSwitchASCII(switches::kX11Display, ":0");
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, EmptyDisplayIsNotAnX11Display) {
  EnvVar display("DISPLAY", "");
  EnvVar wayland_display("WAYLAND_DISPLAY", "wayland-1");
  EXPECT_EQ(PickPlatform(), "wayland");
}

TEST_F(SetOzonePlatformForLinuxTest, EmptyWaylandDisplayIsNotAWaylandDisplay) {
  EnvVar session_type("XDG_SESSION_TYPE", "wayland");
  EnvVar display("DISPLAY", ":0");
  EnvVar wayland_display("WAYLAND_DISPLAY", "");
  CreateDefaultWaylandSocket();
  EXPECT_EQ(PickPlatform(), "x11");
}

TEST_F(SetOzonePlatformForLinuxTest, DoesNotChangeTheEnvironment) {
  EnvVar display("DISPLAY", ":0");
  CreateDefaultWaylandSocket();
  EXPECT_EQ(PickPlatform(), "x11");
  EXPECT_FALSE(base::Environment::Create()->HasVar("WAYLAND_DISPLAY"));
}

}  // namespace ui

#endif  // BUILDFLAG(SUPPORTS_OZONE_WAYLAND) && BUILDFLAG(SUPPORTS_OZONE_X11)

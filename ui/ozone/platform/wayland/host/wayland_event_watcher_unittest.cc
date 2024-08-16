// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>

#include "base/debug/crash_logging.h"
#include "base/environment.h"
#include "base/i18n/number_formatting.h"
#include "base/nix/xdg_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {

namespace {

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

}  // namespace

// Tests that various types of critical errors result in correct crash keys.
//
// Posting the error terminates the client-server connection.  Further attempts
// to sync will hang.  That is why we run "bare" lambdas in all tests of this
// suite instead of using `PostToServerAndWait()`: that helper syncs the
// connection after running the server task.  Due to the same reason we disable
// the sync on the test tear down.  To ensure that the error message is caught
// by the crash reporter, we wait until idle in the end of each test before
// checking the crash key value.
class WaylandEventWatcherTest : public WaylandTestSimple {
 protected:
  void TearDown() override {
    // All tests in this suite terminate the client-server connection by posting
    // various errors.  We cannot sync the connection on tear down.
    DisableSyncOnTearDown();

    WaylandTestSimple::TearDown();
  }
};

TEST_F(WaylandEventWatcherTest, CrashKeyResourceError) {
  const std::string kTestErrorString = "This is a nice error.";

  std::string text;
  auto callback = base::BindLambdaForTesting(
      [&text](const std::string& data) { text = data; });

  server_.RunAndWait(base::BindLambdaForTesting(
      [&kTestErrorString, callback,
       surface_id = window_->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* const xdg_surface = server->GetObject<wl::MockSurface>(surface_id)
                                      ->xdg_surface()
                                      ->resource();

        // Prepare the expectation error string.
        const std::string expected_error_code = base::StrCat(
            {wl_resource_get_class(xdg_surface), "#",
             NumberToString(wl_resource_get_id(xdg_surface)), ": error ",
             NumberToString(
                 static_cast<uint32_t>(XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER)),
             ": ", kTestErrorString});

        callback.Run(expected_error_code);
        wl_resource_post_error(xdg_surface,
                               XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER, "%s",
                               kTestErrorString.c_str());
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(text, crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_F(WaylandEventWatcherTest, CrashKeyResourceNoMemory) {
  // Prepare the expectation error string.
  const std::string expected_error_code = base::StrCat(
      {"wl_display#1: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_NO_MEMORY)),
       ": no memory"});

  server_.RunAndWait(base::BindLambdaForTesting(
      [surface_id = window_->root_surface()->get_surface_id()](
          wl::TestWaylandServerThread* server) {
        auto* const xdg_surface = server->GetObject<wl::MockSurface>(surface_id)
                                      ->xdg_surface()
                                      ->resource();

        wl_resource_post_no_memory(xdg_surface);
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_F(WaylandEventWatcherTest, CrashKeyClientNoMemoryError) {
  const std::string expected_error_code = base::StrCat(
      {"wl_display#1: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_NO_MEMORY)),
       ": no memory"});

  server_.RunAndWait(
      base::BindLambdaForTesting([](wl::TestWaylandServerThread* server) {
        wl_client_post_no_memory(server->client());
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_F(WaylandEventWatcherTest, CrashKeyClientImplementationError) {
  const std::string kError = "A nice error.";
  const std::string expected_error_code = base::StrCat(
      {"wl_display#1: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_IMPLEMENTATION)),
       ": ", kError});

  server_.RunAndWait(base::BindLambdaForTesting(
      [&kError](wl::TestWaylandServerThread* server) {
        wl_client_post_implementation_error(server->client(), "%s",
                                            kError.c_str());
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_F(WaylandEventWatcherTest, CrashKeyCompositorNameSet) {
  const std::string kTestWaylandCompositor = "OzoneWaylandTestCompositor";
  base::Environment::Create()->SetVar(base::nix::kXdgCurrentDesktopEnvVar,
                                      kTestWaylandCompositor);

  server_.RunAndWait(
      base::BindLambdaForTesting([](wl::TestWaylandServerThread* server) {
        wl_client_post_implementation_error(server->client(), "%s",
                                            "stub error");
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestWaylandCompositor,
            crash_reporter::GetCrashKeyValue("wayland_compositor"));
}

TEST_F(WaylandEventWatcherTest, CrashKeyCompositorNameUnset) {
  base::Environment::Create()->UnSetVar(base::nix::kXdgCurrentDesktopEnvVar);

  server_.RunAndWait(
      base::BindLambdaForTesting([](wl::TestWaylandServerThread* server) {
        wl_client_post_implementation_error(server->client(), "%s",
                                            "stub error");
      }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Unknown", crash_reporter::GetCrashKeyValue("wayland_compositor"));
}

}  // namespace ui

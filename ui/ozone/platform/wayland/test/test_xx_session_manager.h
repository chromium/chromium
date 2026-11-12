// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XX_SESSION_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XX_SESSION_MANAGER_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manages xx_session_manager_v1 global. Sessions requested with an id are
// always restored, and the others are created with `kTestSessionId`.
class TestXxSessionManager : public GlobalObject {
 public:
  static constexpr char kTestSessionId[] = "test-session";

  TestXxSessionManager();
  TestXxSessionManager(const TestXxSessionManager&) = delete;
  TestXxSessionManager& operator=(const TestXxSessionManager&) = delete;
  ~TestXxSessionManager() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XX_SESSION_MANAGER_H_

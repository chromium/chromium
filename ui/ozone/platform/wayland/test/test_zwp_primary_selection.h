// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_PRIMARY_SELECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_PRIMARY_SELECTION_H_

#include <memory>

#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

namespace wl {

std::unique_ptr<TestSelectionDeviceManager> CreateTestSelectionManagerZwp();
}

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_PRIMARY_SELECTION_H_

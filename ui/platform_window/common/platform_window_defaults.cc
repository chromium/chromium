// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/common/platform_window_defaults.h"

namespace ui {
namespace {

bool g_use_test_config = false;

}  // namespace

bool UseTestConfigForPlatformWindows() {
  return g_use_test_config;
}

namespace test {

void EnableTestConfigForPlatformWindows() {
  g_use_test_config = true;
}

}  // namespace test
}  // namespace ui

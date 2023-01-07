// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_STYLUS_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_STYLUS_H_

#include <stylus-unstable-v2-server-protocol.h>

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestZcrStylus : public GlobalObject {
 public:
  TestZcrStylus();

  TestZcrStylus(const TestZcrStylus&) = delete;
  TestZcrStylus& operator=(const TestZcrStylus&) = delete;

  ~TestZcrStylus() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_STYLUS_H_

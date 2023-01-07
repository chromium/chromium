// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_TOUCH_STYLUS_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_TOUCH_STYLUS_H_

#include <stylus-unstable-v2-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zcr_touch_stylus_v2_interface kMockZcrTouchStylusImpl;

class MockZcrTouchStylus : public ServerObject {
 public:
  explicit MockZcrTouchStylus(wl_resource* resource);

  MockZcrTouchStylus(const MockZcrTouchStylus&) = delete;
  MockZcrTouchStylus& operator=(const MockZcrTouchStylus&) = delete;

  ~MockZcrTouchStylus() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_TOUCH_STYLUS_H_

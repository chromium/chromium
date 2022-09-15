// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_

#include <text-input-unstable-v1-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct zwp_text_input_v1_interface kMockZwpTextInputV1Impl;

// Manage zwp_text_input_v1.
class MockZwpTextInput : public ServerObject {
 public:
  MockZwpTextInput(wl_resource* resource);

  MockZwpTextInput(const MockZwpTextInput&) = delete;
  MockZwpTextInput& operator=(const MockZwpTextInput&) = delete;

  ~MockZwpTextInput() override;

  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(Activate, void(wl_resource* window));
  MOCK_METHOD0(Deactivate, void());
  MOCK_METHOD0(ShowInputPanel, void());
  MOCK_METHOD0(HideInputPanel, void());
  MOCK_METHOD4(SetCursorRect,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD2(SetSurroundingText,
               void(std::string text, gfx::Range selection_range));
  MOCK_METHOD2(SetContentType,
               void(uint32_t content_hint, uint32_t content_purpose));
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_

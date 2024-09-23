// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_

#include <text-input-unstable-v1-server-protocol.h>
#include <text-input-unstable-v3-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

struct wl_resource;

namespace wl {

extern const struct zwp_text_input_v1_interface kMockZwpTextInputV1Impl;
extern const struct zwp_text_input_v3_interface kMockZwpTextInputV3Impl;

// Manage zwp_text_input_v1.
class MockZwpTextInputV1 : public ServerObject {
 public:
  explicit MockZwpTextInputV1(wl_resource* resource);

  MockZwpTextInputV1(const MockZwpTextInputV1&) = delete;
  MockZwpTextInputV1& operator=(const MockZwpTextInputV1&) = delete;

  ~MockZwpTextInputV1() override;

  void set_text_input_manager(TestZwpTextInputManagerV1* manager) {
    text_input_manager_ = manager;
  }

  MOCK_METHOD(void, Reset, ());
  MOCK_METHOD(void, Activate, (wl_resource * window));
  MOCK_METHOD(void, Deactivate, ());
  MOCK_METHOD(void, ShowInputPanel, ());
  MOCK_METHOD(void, HideInputPanel, ());
  MOCK_METHOD(void,
              SetCursorRect,
              (int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD(void,
              SetSurroundingText,
              (std::string text, gfx::Range selection_range));
  MOCK_METHOD(void,
              SetContentType,
              (uint32_t content_hint, uint32_t content_purpose));

 private:
  raw_ptr<TestZwpTextInputManagerV1> text_input_manager_;
};

class MockZwpTextInputV3 : public ServerObject {
 public:
  explicit MockZwpTextInputV3(wl_resource* resource);

  MockZwpTextInputV3(const MockZwpTextInputV3&) = delete;
  MockZwpTextInputV3& operator=(const MockZwpTextInputV3&) = delete;

  ~MockZwpTextInputV3() override;

  void set_text_input_manager(TestZwpTextInputManagerV3* manager) {
    text_input_manager_ = manager;
  }

  MOCK_METHOD(void, Enable, ());
  MOCK_METHOD(void, Disable, ());
  MOCK_METHOD(void,
              SetSurroundingText,
              (std::string text, gfx::Range selection_range));
  MOCK_METHOD(void,
              SetCursorRect,
              (int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD(void, SetTextChangeCause, (uint32_t cause));
  MOCK_METHOD(void,
              SetContentType,
              (uint32_t content_hint, uint32_t content_purpose));
  MOCK_METHOD(void, Commit, ());

 private:
  raw_ptr<TestZwpTextInputManagerV3> text_input_manager_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_

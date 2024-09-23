// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_

#include <text-input-unstable-v1-server-protocol.h>
#include <text-input-unstable-v3-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct zwp_text_input_manager_v1_interface
    kTestZwpTextInputManagerV1Impl;
extern const struct zwp_text_input_manager_v3_interface
    kTestZwpTextInputManagerV3Impl;

class MockZwpTextInputV1;
class MockZwpTextInputV3;

// Manage zwp_text_input_manager_v1 object.
class TestZwpTextInputManagerV1 : public GlobalObject {
 public:
  TestZwpTextInputManagerV1();

  TestZwpTextInputManagerV1(const TestZwpTextInputManagerV1&) = delete;
  TestZwpTextInputManagerV1& operator=(const TestZwpTextInputManagerV1&) =
      delete;

  ~TestZwpTextInputManagerV1() override;

  void set_text_input(MockZwpTextInputV1* text_input) {
    text_input_ = text_input;
  }
  MockZwpTextInputV1* text_input() const { return text_input_; }

  void OnTextInputDestroyed(MockZwpTextInputV1* text_input);

 private:
  raw_ptr<MockZwpTextInputV1> text_input_ = nullptr;
};

class TestZwpTextInputManagerV3 : public GlobalObject {
 public:
  TestZwpTextInputManagerV3();

  TestZwpTextInputManagerV3(const TestZwpTextInputManagerV3&) = delete;
  TestZwpTextInputManagerV3& operator=(const TestZwpTextInputManagerV3&) =
      delete;

  ~TestZwpTextInputManagerV3() override;

  void set_text_input(MockZwpTextInputV3* text_input) {
    text_input_ = text_input;
  }
  MockZwpTextInputV3* text_input() const { return text_input_; }

  void OnTextInputDestroyed(MockZwpTextInputV3* text_input);

 private:
  raw_ptr<MockZwpTextInputV3> text_input_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_MANAGER_H_

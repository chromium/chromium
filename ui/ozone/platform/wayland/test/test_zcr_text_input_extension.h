// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_TEXT_INPUT_EXTENSION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_TEXT_INPUT_EXTENSION_H_

#include <text-input-extension-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct zcr_text_input_extension_v1_interface
    kTestZcrTextInputExtensionV1Impl;

class MockZcrExtendedTextInput;

// Manage zcr_text_input_extension_v1 object.
class TestZcrTextInputExtensionV1 : public GlobalObject {
 public:
  enum class Version : uint32_t {
    kV7 = 7,
    kV8 = 8,
    kV10 = 10,
    kV12 = 12,
    kV14 = 14,
  };
  explicit TestZcrTextInputExtensionV1(Version version);
  TestZcrTextInputExtensionV1(const TestZcrTextInputExtensionV1&) = delete;
  TestZcrTextInputExtensionV1& operator=(const TestZcrTextInputExtensionV1&) =
      delete;
  ~TestZcrTextInputExtensionV1() override;

  void set_extended_text_input(MockZcrExtendedTextInput* extended_text_input) {
    extended_text_input_ = extended_text_input;
  }
  MockZcrExtendedTextInput* extended_text_input() const {
    return extended_text_input_;
  }

 private:
  raw_ptr<MockZcrExtendedTextInput, DanglingUntriaged> extended_text_input_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZCR_TEXT_INPUT_EXTENSION_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_EXTENDED_TEXT_INPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_EXTENDED_TEXT_INPUT_H_

#include <text-input-extension-unstable-v1-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct zcr_extended_text_input_v1_interface
    kMockZcrExtendedTextInputV1Impl;

// Manage zcr_extended_text_input_v1.
class MockZcrExtendedTextInput : public ServerObject {
 public:
  explicit MockZcrExtendedTextInput(wl_resource* resource);
  MockZcrExtendedTextInput(const MockZcrExtendedTextInput&) = delete;
  MockZcrExtendedTextInput& operator=(const MockZcrExtendedTextInput&) = delete;
  ~MockZcrExtendedTextInput() override;

  // Currently, no mock method is needed.
  // If you're adding new APIs, please see also mock_zwp_text_input.h as
  // reference.
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_V3_CLIENT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_V3_CLIENT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_v3.h"

namespace ui {

class MockZwpTextInputV3Client : public ZwpTextInputV3Client {
 public:
  MockZwpTextInputV3Client();
  MockZwpTextInputV3Client(const MockZwpTextInputV3Client&) = delete;
  MockZwpTextInputV3Client& operator=(const MockZwpTextInputV3Client&) = delete;
  ~MockZwpTextInputV3Client() override;

  MOCK_METHOD(void,
              OnPreeditString,
              (std::string_view text,
               const std::vector<SpanStyle>& spans,
               const gfx::Range& preedit_cursor),
              (override));
  MOCK_METHOD(void, OnCommitString, (std::string_view text), (override));
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_V3_CLIENT_H_

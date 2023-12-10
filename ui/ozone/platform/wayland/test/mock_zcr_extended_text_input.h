// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_EXTENDED_TEXT_INPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_EXTENDED_TEXT_INPUT_H_

#include <text-input-extension-unstable-v1-server-protocol.h>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
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

  MOCK_METHOD(void,
              DeprecatedSetInputType,
              (uint32_t input_type,
               uint32_t input_mode,
               uint32_t input_flags,
               uint32_t learning_mode));
  MOCK_METHOD(void,
              SetInputType,
              (uint32_t input_type,
               uint32_t input_mode,
               uint32_t input_flags,
               uint32_t learning_mode,
               uint32_t inline_composition_support));
  MOCK_METHOD(void,
              SetGrammarFragmentAtCursor,
              (const gfx::Range& range, const std::string& suggestion));
  MOCK_METHOD(void,
              SetAutocorrectInfo,
              (const gfx::Range& range, const gfx::Rect& bounds));
  MOCK_METHOD(void, FinalizeVirtualKeyboardChanges, ());
  MOCK_METHOD(void, SetFocusReason, (uint32_t reason));
  MOCK_METHOD(void, SetSurroundingTextSupport, (uint32_t support));
  MOCK_METHOD(void, SetSurroundingTextOffsetUtf16, (uint32_t offset));
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZCR_EXTENDED_TEXT_INPUT_H_

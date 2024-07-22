// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class MockZWPTextInputWrapperClient : public ZWPTextInputWrapperClient {
 public:
  MockZWPTextInputWrapperClient();
  MockZWPTextInputWrapperClient(const MockZWPTextInputWrapperClient&) = delete;
  MockZWPTextInputWrapperClient& operator=(
      const MockZWPTextInputWrapperClient&) = delete;
  ~MockZWPTextInputWrapperClient() override;

  MOCK_METHOD(void,
              OnPreeditString,
              (std::string_view text,
               const std::vector<SpanStyle>& spans,
               const gfx::Range& preedit_cursor),
              (override));
  MOCK_METHOD(void, OnCommitString, (std::string_view text), (override));
  MOCK_METHOD(void,
              OnCursorPosition,
              (int32_t index, int32_t anchor),
              (override));
  MOCK_METHOD(void,
              OnDeleteSurroundingText,
              (int32_t index, uint32_t length),
              (override));
  MOCK_METHOD(void,
              OnKeysym,
              (uint32_t key, uint32_t state, uint32_t modifiers, uint32_t time),
              (override));
  MOCK_METHOD(void,
              OnSetPreeditRegion,
              (int32_t index,
               uint32_t length,
               const std::vector<SpanStyle>& spans),
              (override));
  MOCK_METHOD(void,
              OnClearGrammarFragments,
              (const gfx::Range& range),
              (override));
  MOCK_METHOD(void,
              OnAddGrammarFragment,
              (const ui::GrammarFragment& fragment),
              (override));
  MOCK_METHOD(void,
              OnSetAutocorrectRange,
              (const gfx::Range& range),
              (override));
  MOCK_METHOD(void,
              OnSetVirtualKeyboardOccludedBounds,
              (const gfx::Rect& screen_bounds),
              (override));
  MOCK_METHOD(void, OnConfirmPreedit, (bool keep_selection), (override));
  MOCK_METHOD(void, OnInputPanelState, (uint32_t state), (override));
  MOCK_METHOD(void,
              OnModifiersMap,
              (std::vector<std::string> modifiers_map),
              (override));
  MOCK_METHOD(void, OnInsertImage, (const GURL& src), (override));
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_

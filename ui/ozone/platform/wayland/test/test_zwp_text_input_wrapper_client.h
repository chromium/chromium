// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class TestZWPTextInputWrapperClient : public ZWPTextInputWrapperClient {
 public:
  TestZWPTextInputWrapperClient() = default;
  TestZWPTextInputWrapperClient(const TestZWPTextInputWrapperClient&) = delete;
  TestZWPTextInputWrapperClient& operator=(
      const TestZWPTextInputWrapperClient&) = delete;
  ~TestZWPTextInputWrapperClient() override = default;

  void OnPreeditString(std::string_view text,
                       const std::vector<SpanStyle>& spans,
                       const gfx::Range& preedit_cursor) override;
  void OnCommitString(std::string_view text) override;
  void OnCursorPosition(int32_t index, int32_t anchor) override;
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override;
  void OnKeysym(uint32_t key,
                uint32_t state,
                uint32_t modifiers,
                uint32_t time) override;
  void OnSetPreeditRegion(int32_t index,
                          uint32_t length,
                          const std::vector<SpanStyle>& spans) override;
  void OnClearGrammarFragments(const gfx::Range& range) override;
  void OnAddGrammarFragment(const ui::GrammarFragment& fragment) override;
  void OnSetAutocorrectRange(const gfx::Range& range) override;
  void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) override;
  void OnConfirmPreedit(bool keep_selection) override;
  void OnInputPanelState(uint32_t state) override;
  void OnModifiersMap(std::vector<std::string> modifiers_map) override;
  void OnInsertImage(const GURL& src) override;

  GURL last_inserted_image_url() const { return last_inserted_image_url_; }
  uint32_t last_keysym_time() const { return last_keysym_time_; }

 private:
  GURL last_inserted_image_url_;
  uint32_t last_keysym_time_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_TEXT_INPUT_WRAPPER_CLIENT_H_

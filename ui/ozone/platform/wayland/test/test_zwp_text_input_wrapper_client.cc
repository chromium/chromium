// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_text_input_wrapper_client.h"

namespace ui {

void TestZWPTextInputWrapperClient::OnPreeditString(
    std::string_view text,
    const std::vector<SpanStyle>& spans,
    const gfx::Range& preedit_cursor) {}

void TestZWPTextInputWrapperClient::OnCommitString(std::string_view text) {}

void TestZWPTextInputWrapperClient::OnCursorPosition(int32_t index,
                                                     int32_t anchor) {}

void TestZWPTextInputWrapperClient::OnDeleteSurroundingText(int32_t index,
                                                            uint32_t length) {}

void TestZWPTextInputWrapperClient::OnKeysym(uint32_t key,
                                             uint32_t state,
                                             uint32_t modifiers,
                                             uint32_t time) {
  last_keysym_time_ = time;
}

void TestZWPTextInputWrapperClient::OnSetPreeditRegion(
    int32_t index,
    uint32_t length,
    const std::vector<SpanStyle>& spans) {}

void TestZWPTextInputWrapperClient::OnClearGrammarFragments(
    const gfx::Range& range) {}

void TestZWPTextInputWrapperClient::OnAddGrammarFragment(
    const ui::GrammarFragment& fragment) {}

void TestZWPTextInputWrapperClient::OnSetAutocorrectRange(
    const gfx::Range& range) {}

void TestZWPTextInputWrapperClient::OnSetVirtualKeyboardOccludedBounds(
    const gfx::Rect& screen_bounds) {}

void TestZWPTextInputWrapperClient::OnConfirmPreedit(bool keep_selection) {}

void TestZWPTextInputWrapperClient::OnInputPanelState(uint32_t state) {}

void TestZWPTextInputWrapperClient::OnModifiersMap(
    std::vector<std::string> modifiers_map) {}

void TestZWPTextInputWrapperClient::OnInsertImage(const GURL& src) {
  last_inserted_image_url_ = src;
}

}  // namespace ui

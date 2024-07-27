// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "ui/base/ime/grammar_fragment.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

class WaylandWindow;

// Client interface which handles wayland text input callbacks
class ZWPTextInputWrapperClient {
 public:
  struct SpanStyle {
    struct Style {
      bool operator==(const Style& other) const = default;

      ImeTextSpan::Type type;
      ImeTextSpan::Thickness thickness;
    };

    bool operator==(const SpanStyle& other) const = default;

    // Byte offset.
    uint32_t index;
    // Length in bytes.
    uint32_t length;
    // One of preedit_style.
    std::optional<Style> style;
  };

  virtual ~ZWPTextInputWrapperClient() = default;

  // Called when a new composing text (pre-edit) should be set around the
  // current cursor position. Any previously set composing text should
  // be removed.
  // Note that the preedit_cursor is byte-offset. It is the pre-edit cursor
  // position if the range is empty and selection otherwise.
  virtual void OnPreeditString(std::string_view text,
                               const std::vector<SpanStyle>& spans,
                               const gfx::Range& preedit_cursor) = 0;

  // Called when a complete input sequence has been entered.  The text to
  // commit could be either just a single character after a key press or the
  // result of some composing (pre-edit).
  virtual void OnCommitString(std::string_view text) = 0;

  // Called when the cursor position or selection should be modified. The new
  // cursor position is applied on the next OnCommitString. |index| and |anchor|
  // are measured in UTF-8 bytes.
  virtual void OnCursorPosition(int32_t index, int32_t anchor) = 0;

  // Called when client needs to delete all or part of the text surrounding
  // the cursor. |index| and |length| are expected to be a byte offset of |text|
  // passed via ZWPTextInputWrapper::SetSurroundingText.
  virtual void OnDeleteSurroundingText(int32_t index, uint32_t length) = 0;

  // Notify when a key event was sent. Key events should not be used
  // for normal text input operations, which should be done with
  // commit_string, delete_surrounding_text, etc.
  virtual void OnKeysym(uint32_t key,
                        uint32_t state,
                        uint32_t modifiers,
                        uint32_t time) = 0;

  // Called when a new preedit region is specified. The region is specified
  // by |index| and |length| on the surrounding text sent do wayland compositor
  // in advance. |index| is relative to the current caret position, and |length|
  // is the preedit length. Both are measured in UTF-8 bytes.
  // |spans| are representing the text spanning for the new preedit. All its
  // indices and length are relative to the beginning of the new preedit,
  // and measured in UTF-8 bytes.
  virtual void OnSetPreeditRegion(int32_t index,
                                  uint32_t length,
                                  const std::vector<SpanStyle>& spans) = 0;

  // Called when client needs to clear all grammar fragments in |range|. All
  // indices are measured in UTF-8 bytes.
  virtual void OnClearGrammarFragments(const gfx::Range& range) = 0;

  // Called when client requests to add a new grammar marker. All indices are
  // measured in UTF-8 bytes.
  virtual void OnAddGrammarFragment(const ui::GrammarFragment& fragment) = 0;

  // Sets the autocorrect range in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetAutocorrectRange(const gfx::Range& range) = 0;

  // Called when the virtual keyboard's occluded bounds is updated.
  // The bounds are in screen DIP.
  virtual void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) = 0;

  // Called when confirming the preedit.
  virtual void OnConfirmPreedit(bool keep_selection) = 0;

  // Called when the visibility state of the input panel changed.
  // There's no detailed spec of |state|, and no actual implementor except
  // components/exo is found in the world at this moment.
  // Thus, in ozone/wayland use the lowest bit as boolean
  // (visible=1/invisible=0), and ignore other bits for future compatibility.
  // This behavior must be consistent with components/exo.
  virtual void OnInputPanelState(uint32_t state) = 0;

  // Called when the modifiers map is updated.
  // Each element holds the XKB name represents a modifier, such as "Shift".
  // The position of the element represents the bit position of modifiers
  // on OnKeysym. E.g., if LSB of modifiers is set, modifiers_map[0] is
  // set, if (1 << 1) of modifiers is set, modifiers_map[1] is set, and so on.
  virtual void OnModifiersMap(std::vector<std::string> modifiers_map) = 0;

  // Called when some image is being inserted.
  virtual void OnInsertImage(const GURL& src) = 0;
};

// A wrapper around different versions of wayland text input protocols.
// Wayland compositors support various different text input protocols which
// all from Chromium point of view provide the functionality needed by Chromium
// IME. This interface collects the functionality behind one wrapper API.
class ZWPTextInputWrapper {
 public:
  virtual ~ZWPTextInputWrapper() = default;

  virtual void Reset() = 0;

  virtual void Activate(WaylandWindow* window,
                        ui::TextInputClient::FocusReason reason) = 0;
  virtual void Deactivate() = 0;

  virtual void ShowInputPanel() = 0;
  virtual void HideInputPanel() = 0;

  virtual void SetCursorRect(const gfx::Rect& rect) = 0;
  virtual void SetSurroundingText(const std::string& text,
                                  const gfx::Range& preedit_range,
                                  const gfx::Range& selection_range) = 0;
  virtual bool HasAdvancedSurroundingTextSupport() const = 0;
  virtual void SetSurroundingTextOffsetUtf16(uint32_t offset_utf16) = 0;
  virtual void SetContentType(ui::TextInputType type,
                              ui::TextInputMode mode,
                              uint32_t flags,
                              bool should_do_learning,
                              bool can_compose_inline) = 0;

  virtual void SetGrammarFragmentAtCursor(
      const ui::GrammarFragment& fragment) = 0;
  virtual void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                                  const gfx::Rect& autocorrect_bounds) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_

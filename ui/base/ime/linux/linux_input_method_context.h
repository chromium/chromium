// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/autocorrect_info.h"
#include "ui/base/ime/grammar_fragment.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

struct CompositionText;
class KeyEvent;
struct ImeTextSpan;
class VirtualKeyboardController;

// An interface of input method context for input method frameworks on
// GNU/Linux and likes.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContext {
 public:
  struct TextInputClientAttributes {
    TextInputType input_type = TEXT_INPUT_TYPE_NONE;
    TextInputMode input_mode = TEXT_INPUT_MODE_DEFAULT;
    uint32_t flags = TEXT_INPUT_FLAG_NONE;
    bool should_do_learning = false;
    bool can_compose_inline = true;
  };

  virtual ~LinuxInputMethodContext() = default;

  // Dispatches the key event to an underlying IME.  Returns true if the key
  // event is handled, otherwise false.  A client must set the text input type
  // before dispatching a key event.
  virtual bool DispatchKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Returns whether the event is a peek key event.
  virtual bool IsPeekKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Takes cursor rect in screen coordinates. Result used by system IME.
  virtual void SetCursorLocation(const gfx::Rect& rect) = 0;

  // Tells the system IME the surrounding text around the cursor location.
  virtual void SetSurroundingText(
      const std::u16string& text,
      const gfx::Range& text_range,
      const gfx::Range& composition_range,
      const gfx::Range& selection_range,
      const std::optional<ui::GrammarFragment>& fragment,
      const std::optional<AutocorrectInfo>& autocorrect) = 0;

  // Resets the context.  A client needs to call OnTextInputTypeChanged() again
  // before calling DispatchKeyEvent().
  virtual void Reset() = 0;

  // Called when the text input focus is about to change.
  virtual void WillUpdateFocus(TextInputClient* old_client,
                               TextInputClient* new_client) {}

  // Called when text input focus is changed.
  virtual void UpdateFocus(
      bool has_client,
      TextInputType old_type,
      const TextInputClientAttributes& new_client_attributes,
      TextInputClient::FocusReason reason) = 0;

  // Returns the corresponding VirtualKeyboardController instance.
  // Or nullptr, if not supported.
  virtual VirtualKeyboardController* GetVirtualKeyboardController() = 0;
};

// An interface of callback functions called from LinuxInputMethodContext.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContextDelegate {
 public:
  virtual ~LinuxInputMethodContextDelegate() {}

  // Commits the |text| to the text input client.
  virtual void OnCommit(const std::u16string& text) = 0;

  // Converts current composition text into final content.
  virtual void OnConfirmCompositionText(bool keep_selection) = 0;

  // Deletes the surrounding text around selection. |before| and |after|
  // are in UTF-16 code points.
  virtual void OnDeleteSurroundingText(size_t before, size_t after) = 0;

  // Sets the composition text to the text input client.
  virtual void OnPreeditChanged(const CompositionText& composition_text) = 0;

  // Cleans up a composition session and makes sure that the composition text is
  // cleared.
  virtual void OnPreeditEnd() = 0;

  // Prepares things for a new composition session.
  virtual void OnPreeditStart() = 0;

  // Sets the composition from the current text in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetPreeditRegion(const gfx::Range& range,
                                  const std::vector<ImeTextSpan>& spans) = 0;

  // Clears all the grammar fragments in |range|. All indices are measured in
  // UTF-16 code point.
  virtual void OnClearGrammarFragments(const gfx::Range& range) = 0;

  // Adds a new grammar marker according to |fragments|. Clients should show
  // some visual indications such as underlining. All indices are measured in
  // UTF-16 code point.
  virtual void OnAddGrammarFragment(const ui::GrammarFragment& fragment) = 0;

  // Sets the autocorrect range in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetAutocorrectRange(const gfx::Range& range) = 0;

  // Sets the virtual keyboard's occluded bounds in screen DIP.
  virtual void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) = 0;

  // Inserts an image.
  virtual void OnInsertImage(const GURL& src) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_

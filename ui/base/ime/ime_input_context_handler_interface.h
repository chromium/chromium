// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_
#define UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_

#include <stdint.h>

#include <string>
#include "base/component_export.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"

namespace ui {

struct SurroundingTextInfo {
  base::string16 surrounding_text;
  gfx::Range selection_range;
};

class COMPONENT_EXPORT(UI_BASE_IME) IMEInputContextHandlerInterface {
 public:
  // Called when the engine commit a text.
  virtual void CommitText(const std::string& text) = 0;

#if defined(OS_CHROMEOS)
  // Called when the engine changes the composition range.
  // Returns true if the operation was successful.
  virtual bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) = 0;

  // Called when the engine changes the selection range.
  // Returns true if the operation was successful.
  virtual bool SetSelectionRange(uint32_t start, uint32_t end) = 0;
#endif

  // Called when the engine updates composition text.
  virtual void UpdateCompositionText(const CompositionText& text,
                                     uint32_t cursor_pos,
                                     bool visible) = 0;

  // Called when the engine request deleting surrounding string.
  virtual void DeleteSurroundingText(int32_t offset, uint32_t length) = 0;

  // Called from the extension API.
  virtual SurroundingTextInfo GetSurroundingTextInfo() = 0;

  // Called when the engine sends a key event.
  virtual void SendKeyEvent(KeyEvent* event) = 0;

  // Gets the input method pointer.
  virtual InputMethod* GetInputMethod() = 0;

  // Commits any composition text.
  // Set |reset_engine| to false if this was triggered from the extension.
  virtual void ConfirmCompositionText(bool reset_engine,
                                      bool keep_selection) = 0;

  // Returns true if there is any composition text.
  virtual bool HasCompositionText() = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_

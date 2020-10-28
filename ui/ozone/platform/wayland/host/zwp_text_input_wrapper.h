// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_

#include <stdint.h>

#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Client interface which handles wayland text input callbacks
class ZWPTextInputWrapperClient {
 public:
  struct SpanStyle {
    uint32_t index;   // Byte offset.
    uint32_t length;  // Length in bytes.
    uint32_t style;   // One of preedit_style.
  };

  virtual ~ZWPTextInputWrapperClient() = default;

  // Called when a new composing text (pre-edit) should be set around the
  // current cursor position. Any previously set composing text should
  // be removed.
  // Note that the preedit_cursor is byte-offset.
  virtual void OnPreeditString(base::StringPiece text,
                               const std::vector<SpanStyle>& spans,
                               int32_t preedit_cursor) = 0;

  // Called when a complete input sequence has been entered.  The text to
  // commit could be either just a single character after a key press or the
  // result of some composing (pre-edit).
  virtual void OnCommitString(base::StringPiece text) = 0;

  // Called when client needs to delete all or part of the text surrounding
  // the cursor
  virtual void OnDeleteSurroundingText(int32_t index, uint32_t length) = 0;

  // Notify when a key event was sent. Key events should not be used
  // for normal text input operations, which should be done with
  // commit_string, delete_surrounding_text, etc.
  virtual void OnKeysym(uint32_t key, uint32_t state, uint32_t modifiers) = 0;
};

// A wrapper around different versions of wayland text input protocols.
// Wayland compositors support various different text input protocols which
// all from Chromium point of view provide the functionality needed by Chromium
// IME. This interface collects the functionality behind one wrapper API.
class ZWPTextInputWrapper {
 public:
  virtual ~ZWPTextInputWrapper() = default;

  virtual void Initialize(WaylandConnection* connection,
                          ZWPTextInputWrapperClient* client) = 0;

  virtual void Reset() = 0;

  virtual void Activate(WaylandWindow* window) = 0;
  virtual void Deactivate() = 0;

  virtual void ShowInputPanel() = 0;
  virtual void HideInputPanel() = 0;

  virtual void SetCursorRect(const gfx::Rect& rect) = 0;
  virtual void SetSurroundingText(const base::string16& text,
                                  const gfx::Range& selection_range) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_H_

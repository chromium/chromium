// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class WaylandConnection;
class ZWPTextInputWrapper;

class WaylandInputMethodContext : public LinuxInputMethodContext,
                                  public ZWPTextInputWrapperClient {
 public:
  class Delegate;

  WaylandInputMethodContext(WaylandConnection* connection,
                            WaylandKeyboard::Delegate* key_delegate,
                            LinuxInputMethodContextDelegate* ime_delegate,
                            bool is_simple);
  ~WaylandInputMethodContext() override;

  void Init(bool initialize_for_testing = false);

  // LinuxInputMethodContext overrides:
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;

  // ui::ZWPTextInputWrapperClient
  void OnPreeditString(base::StringPiece text,
                       const std::vector<SpanStyle>& spans,
                       int32_t preedit_cursor) override;
  void OnCommitString(base::StringPiece text) override;
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override;
  void OnKeysym(uint32_t keysym, uint32_t state, uint32_t modifiers) override;

 private:
  void UpdatePreeditText(const base::string16& preedit_text);

  WaylandConnection* const connection_;  // TODO(jani) Handle this better

  // Delegate key events to be injected into PlatformEvent system.
  WaylandKeyboard::Delegate* const key_delegate_;

  // Delegate IME-specific events to be handled by //ui code.
  LinuxInputMethodContextDelegate* const ime_delegate_;
  bool is_simple_;

  std::unique_ptr<ZWPTextInputWrapper> text_input_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  CharacterComposer character_composer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContext);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_

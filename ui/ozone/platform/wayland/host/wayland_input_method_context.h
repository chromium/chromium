// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class WaylandConnection;
class ZWPTextInputWrapper;

class WaylandInputMethodContext : public LinuxInputMethodContext,
                                  public ZWPTextInputWrapperClient {
 public:
  WaylandInputMethodContext(WaylandConnection* connection,
                            LinuxInputMethodContextDelegate* delegate,
                            bool is_simple,
                            const EventDispatchCallback& callback);
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
  void OnPreeditString(const std::string& text, int preedit_cursor) override;
  void OnCommitString(const std::string& text) override;
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override;
  void OnKeysym(uint32_t key, uint32_t state, uint32_t modifiers) override;

 private:
  void UpdatePreeditText(const base::string16& preedit_text);

  WaylandConnection* connection_ = nullptr;  // TODO(jani) Handle this better

  // Delegate interface back to IME code in ui.
  LinuxInputMethodContextDelegate* delegate_;
  bool is_simple_;
  EventDispatchCallback callback_;

  std::unique_ptr<ZWPTextInputWrapper> text_input_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  CharacterComposer character_composer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContext);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_

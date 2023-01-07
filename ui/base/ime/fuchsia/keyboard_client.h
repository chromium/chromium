// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_
#define UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_

#include <fuchsia/ui/input3/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/component_export.h"

namespace ui {

class InputEventSink;

// Handles keyboard events from the Fuchsia keyboard service.
class COMPONENT_EXPORT(UI_BASE_IME_FUCHSIA) KeyboardClient
    : public fuchsia::ui::input3::KeyboardListener {
 public:
  // |keyboard_service| and |event_sink| must outlive |this|.
  KeyboardClient(fuchsia::ui::input3::Keyboard* keyboard_service,
                 fuchsia::ui::views::ViewRef view_ref,
                 InputEventSink* event_sink);
  ~KeyboardClient() override;

  KeyboardClient(const KeyboardClient&) = delete;
  KeyboardClient& operator=(const KeyboardClient&) = delete;

  // fuchsia::ui::input3::KeyboardListener implementation.
  void OnKeyEvent(
      fuchsia::ui::input3::KeyEvent key_event,
      fuchsia::ui::input3::KeyboardListener::OnKeyEventCallback callback) final;

 private:
  bool IsValid(const fuchsia::ui::input3::KeyEvent& key_event);

  // Handles converting and propagating |key_event|. Returns false if critical
  // information about |key_event| is missing, or if the key's event type is not
  // supported.
  // TODO(http://fxbug.dev/69620): Add support for SYNC and CANCEL key event
  // types.
  bool ProcessKeyEvent(const fuchsia::ui::input3::KeyEvent& key_event);

  // Update the value of modifiers such as shift.
  void UpdateCachedModifiers(const fuchsia::ui::input3::KeyEvent& key_event);

  // Translate state of locally tracked modifier keys (e.g. shift, alt) into
  // ui::Event flags.
  int EventFlagsForCachedModifiers();

  fidl::Binding<fuchsia::ui::input3::KeyboardListener> binding_;

  // Dispatches events into Chromium once they have been converted to
  // ui::KeyEvents.
  InputEventSink* const event_sink_;

  // Tracks the activation state of the named modifier keys.
  bool left_shift_ = false;
  bool right_shift_ = false;
  bool left_alt_ = false;
  bool right_alt_ = false;
  bool left_ctrl_ = false;
  bool right_ctrl_ = false;
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_

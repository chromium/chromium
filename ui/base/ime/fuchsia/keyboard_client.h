// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_
#define UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_

#include <fidl/fuchsia.ui.input3/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <optional>

#include "base/component_export.h"

namespace ui {

class InputEventSink;

// Handles keyboard events from the Fuchsia keyboard service.
class COMPONENT_EXPORT(UI_BASE_IME_FUCHSIA) KeyboardClient
    : public fidl::Server<fuchsia_ui_input3::KeyboardListener> {
 public:
  // `keyboard_service` and `event_sink` must outlive `this`.
  KeyboardClient(
      fidl::Client<fuchsia_ui_input3::Keyboard>& keyboard_fidl_client,
      fuchsia_ui_views::ViewRef view_ref,
      InputEventSink* event_sink);
  ~KeyboardClient() override;

  KeyboardClient(const KeyboardClient&) = delete;
  KeyboardClient& operator=(const KeyboardClient&) = delete;

  // fuchsia_ui_input3::KeyboardListener implementation.
  void OnKeyEvent(OnKeyEventRequest& request,
                  OnKeyEventCompleter::Sync& completer) final;

 private:
  bool IsValid(const fuchsia_ui_input3::KeyEvent& key_event);

  // Handles converting and propagating `key_event`. Returns false if critical
  // information about `key_event` is missing, or if the key's event type is not
  // supported.
  // TODO(http://fxbug.dev/69620): Add support for SYNC and CANCEL key event
  // types.
  bool ProcessKeyEvent(const fuchsia_ui_input3::KeyEvent& key_event);

  // Update the value of modifiers such as shift.
  void UpdateCachedModifiers(const fuchsia_ui_input3::KeyEvent& key_event);

  // Translate state of locally tracked modifier keys (e.g. shift, alt) into
  // ui::Event flags.
  int EventFlagsForCachedModifiers();

  std::optional<fidl::ServerBinding<fuchsia_ui_input3::KeyboardListener>>
      binding_;

  // Dispatches events into Chromium once they have been converted to
  // ui::KeyEvents.
  InputEventSink* const event_sink_;
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_KEYBOARD_CLIENT_H_

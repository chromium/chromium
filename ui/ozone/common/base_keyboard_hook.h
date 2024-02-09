// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_BASE_KEYBOARD_HOOK_H_
#define UI_OZONE_COMMON_BASE_KEYBOARD_HOOK_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "ui/ozone/public/platform_keyboard_hook.h"

namespace ui {

enum class DomCode : uint32_t;
class KeyEvent;

// Base implementation of KeyboardHook for Ozone platforms.
//
// Holds and provides to the subclasses the set of DOM codes and the callback
// given upon construction.
//
// Can be used as is, and provides the browser-level lock for the given set of
// keys.  Platforms may implement system-level lock.  The instance of this class
// should enable the lock in constructor and release in in the destructor.
class BaseKeyboardHook : public PlatformKeyboardHook {
 public:
  using KeyEventCallback = base::RepeatingCallback<void(KeyEvent* event)>;

  BaseKeyboardHook(std::optional<base::flat_set<DomCode>> dom_codes,
                   KeyEventCallback callback);
  BaseKeyboardHook(const BaseKeyboardHook&) = delete;
  BaseKeyboardHook& operator=(const BaseKeyboardHook&) = delete;
  ~BaseKeyboardHook() override;

  // BaseKeyboardHook:
  bool IsKeyLocked(DomCode dom_code) const override;

 protected:
  // Indicates whether |dom_code| should be intercepted by the keyboard hook.
  bool ShouldCaptureKeyEvent(DomCode dom_code) const;
  // Forwards the key event using |key_event_callback_|.
  // |event| is owned by the calling method and will live until this method
  // returns.
  void ForwardCapturedKeyEvent(KeyEvent* event);
  const std::optional<base::flat_set<DomCode>>& dom_codes() {
    return dom_codes_;
  }

 private:
  // Used to forward key events.
  KeyEventCallback key_event_callback_;
  // The set of keys which should be intercepted by the keyboard hook.
  std::optional<base::flat_set<DomCode>> dom_codes_;
};

}  // namespace ui
#endif  // UI_OZONE_COMMON_BASE_KEYBOARD_HOOK_H_

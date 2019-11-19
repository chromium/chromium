// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_TEXT_EDIT_KEY_BINDINGS_DELEGATE_AURALINUX_H_
#define UI_BASE_IME_LINUX_TEXT_EDIT_KEY_BINDINGS_DELEGATE_AURALINUX_H_

#include <vector>

#include "base/component_export.h"

namespace ui {
class Event;
class TextEditCommandAuraLinux;

// An interface which can interpret various text editing commands out of key
// events.
//
// On desktop Linux, we've traditionally supported the user's custom
// keybindings. We need to support this in both content/ and in views/.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) TextEditKeyBindingsDelegateAuraLinux {
 public:
  // Matches a key event against the users' platform specific key bindings,
  // false will be returned if the key event doesn't correspond to a predefined
  // key binding.  Edit commands matched with |event| will be stored in
  // |edit_commands|, if |edit_commands| is non-NULL.
  virtual bool MatchEvent(const ui::Event& event,
                          std::vector<TextEditCommandAuraLinux>* commands) = 0;

 protected:
  virtual ~TextEditKeyBindingsDelegateAuraLinux() {}
};

// Sets/Gets the global TextEditKeyBindingsDelegateAuraLinux. No ownership
// changes. Can be NULL.
COMPONENT_EXPORT(UI_BASE_IME_LINUX)
void SetTextEditKeyBindingsDelegate(
    TextEditKeyBindingsDelegateAuraLinux* delegate);
COMPONENT_EXPORT(UI_BASE_IME_LINUX)
TextEditKeyBindingsDelegateAuraLinux* GetTextEditKeyBindingsDelegate();

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_TEXT_EDIT_KEY_BINDINGS_DELEGATE_AURALINUX_H_

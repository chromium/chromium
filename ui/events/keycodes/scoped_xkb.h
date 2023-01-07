// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_SCOPED_XKB_H_
#define UI_EVENTS_KEYCODES_SCOPED_XKB_H_

#include <xkbcommon/xkbcommon.h>

namespace ui {

// libxkbcommon uses explicit reference counting for its structures,
// so we need to trigger its cleanup.
struct XkbContextDeleter {
  void operator()(xkb_context* context) { xkb_context_unref(context); }
};

struct XkbStateDeleter {
  void operator()(xkb_state* state) { xkb_state_unref(state); }
};

struct XkbKeymapDeleter {
  void operator()(xkb_keymap* keymap) { xkb_keymap_unref(keymap); }
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_SCOPED_XKB_H_

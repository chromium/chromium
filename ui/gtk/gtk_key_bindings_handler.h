// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_
#define UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_

#include "base/containers/fixed_flat_map.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event.h"
#include "ui/events/platform_event.h"

using GSettings = struct _GSettings;

namespace gtk {

// Translates key events into edit commands based on the key theme.
// Currently only supports the "Emacs" key theme since that's all GTK supports.
// TODO(thomasanderson): Extract this class to be used by all Linux UIs since
// the implementation is not GTK specific.
class GtkKeyBindingsHandler {
 public:
  GtkKeyBindingsHandler();
  ~GtkKeyBindingsHandler();

  ui::TextEditCommand MatchEvent(const ui::Event& event);

 private:
  void OnSettingsChanged(GSettings* settings, const char* key);

  ScopedGObject<GSettings> settings_;
  ScopedGSignal signal_;
  bool emacs_theme_ = false;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_KEY_BINDINGS_HANDLER_H_

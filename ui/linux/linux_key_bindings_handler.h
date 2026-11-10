// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_KEY_BINDINGS_HANDLER_H_
#define UI_LINUX_LINUX_KEY_BINDINGS_HANDLER_H_

#include "base/memory/weak_ptr.h"

#if defined(USE_GSETTINGS)
#include "ui/base/glib/scoped_gobject.h"
#include "ui/base/glib/scoped_gsignal.h"

using GSettings = struct _GSettings;
#endif

namespace ui {

class Event;
enum class TextEditCommand;

// Translates key events into edit commands based on the GSettings key theme.
// Currently only supports the "Emacs" key theme since that is all GTK supports.
class LinuxKeyBindingsHandler {
 public:
  LinuxKeyBindingsHandler();
#if defined(USE_GSETTINGS)
  explicit LinuxKeyBindingsHandler(ScopedGObject<GSettings> settings);
#endif
  LinuxKeyBindingsHandler(const LinuxKeyBindingsHandler&) = delete;
  LinuxKeyBindingsHandler& operator=(const LinuxKeyBindingsHandler&) = delete;
  ~LinuxKeyBindingsHandler();

  TextEditCommand MatchEvent(const Event& event);

 private:
  bool emacs_theme_ = false;
#if defined(USE_GSETTINGS)
  void OnSettingsChanged(GSettings* settings, const char* key);

  ScopedGObject<GSettings> settings_;
  ScopedGSignal signal_;
  base::WeakPtrFactory<LinuxKeyBindingsHandler> weak_factory_{this};
#endif
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_KEY_BINDINGS_HANDLER_H_

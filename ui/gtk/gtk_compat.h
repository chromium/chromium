// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_COMPAT_H_
#define UI_GTK_GTK_COMPAT_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/gtk/gtk_types.h"

extern "C" {
#include "ui/gtk/gdk.sigs"
#include "ui/gtk/gdk_pixbuf.sigs"
#include "ui/gtk/gsk.sigs"
#include "ui/gtk/gtk.sigs"
}

namespace gtk {

// Loads libgtk and related libraries and returns true on success.
COMPONENT_EXPORT(GTK) bool LoadGtk(int gtk_version);

// Returns true iff the runtime version of Gtk used meets
// |major|.|minor|.|micro|. LoadGtk() must have been called
// prior to calling this function.
bool GtkCheckVersion(int major, int minor = 0, int micro = 0);

// The below functions replace GTK functions whose interface has
// changed across versions, but whose (symbol) names have not.

void GtkInit(const std::vector<std::string>& args);

}  // namespace gtk

#endif

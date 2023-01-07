// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_
#define UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_

#include "ui/gfx/geometry/size.h"

namespace printing {
class PrintingContextLinux;
class PrintSettings;
}  // namespace printing

typedef struct _GtkPrintSettings GtkPrintSettings;
typedef struct _GtkPageSetup GtkPageSetup;

// Obtains the paper size through Gtk.
gfx::Size GetPdfPaperSizeDeviceUnitsGtk(
    printing::PrintingContextLinux* context);

// Initializes a PrintSettings object from the provided Gtk printer objects.
void InitPrintSettingsGtk(GtkPrintSettings* settings,
                          GtkPageSetup* page_setup,
                          printing::PrintSettings* print_settings);

#endif  // UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_

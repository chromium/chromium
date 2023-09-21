// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_
#define UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_

#include "printing/buildflags/buildflags.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "base/memory/raw_ptr.h"
#include "ui/gtk/gtk_compat.h"
#endif

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

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
class ScopedGKeyFile {
 public:
  explicit ScopedGKeyFile(GKeyFile* key_file);
  ~ScopedGKeyFile();

  GKeyFile* get() { return key_file_; }

 private:
  raw_ptr<GKeyFile> key_file_;
};
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

#endif  // UI_GTK_PRINTING_PRINTING_GTK_UTIL_H_

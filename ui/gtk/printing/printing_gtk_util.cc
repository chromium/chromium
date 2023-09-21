// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/printing/printing_gtk_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"
#include "printing/printing_context_linux.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gtk/gtk_compat.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include <utility>

#include "base/values.h"
#endif

gfx::Size GetPdfPaperSizeDeviceUnitsGtk(
    printing::PrintingContextLinux* context) {
  GtkPageSetup* page_setup = gtk_page_setup_new();

  gfx::SizeF paper_size(
      gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH),
      gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH));

  g_object_unref(page_setup);

  const printing::PrintSettings& settings = context->settings();

  return gfx::Size(paper_size.width() * settings.device_units_per_inch(),
                   paper_size.height() * settings.device_units_per_inch());
}

void InitPrintSettingsGtk(GtkPrintSettings* settings,
                          GtkPageSetup* page_setup,
                          printing::PrintSettings* print_settings) {
  DCHECK(settings);
  DCHECK(page_setup);
  DCHECK(print_settings);

  const char* printer_name = gtk_print_settings_get_printer(settings);
  std::u16string name =
      printer_name ? base::UTF8ToUTF16(printer_name) : std::u16string();
  print_settings->set_device_name(name);

  gfx::Size physical_size_device_units;
  gfx::Rect printable_area_device_units;
  int dpi = gtk_print_settings_get_resolution(settings);
  CHECK(dpi);
  // Initialize `page_setup_device_units_`.
  physical_size_device_units.SetSize(
      gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH) * dpi,
      gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH) * dpi);
  printable_area_device_units.SetRect(
      gtk_page_setup_get_left_margin(page_setup, GTK_UNIT_INCH) * dpi,
      gtk_page_setup_get_top_margin(page_setup, GTK_UNIT_INCH) * dpi,
      gtk_page_setup_get_page_width(page_setup, GTK_UNIT_INCH) * dpi,
      gtk_page_setup_get_page_height(page_setup, GTK_UNIT_INCH) * dpi);

  print_settings->set_dpi(dpi);

  // Note: With the normal GTK print dialog, when the user selects the landscape
  // orientation, all that does is change the paper size. Which seems to be
  // enough to render the right output and send it to the printer.
  // The orientation value stays as portrait and does not actually affect
  // printing.
  // Thus this is only useful in print preview mode, where we manually set the
  // orientation and change the paper size ourselves.
  GtkPageOrientation orientation = gtk_print_settings_get_orientation(settings);
  // Set before SetPrinterPrintableArea to make it flip area if necessary.
  print_settings->SetOrientation(orientation == GTK_PAGE_ORIENTATION_LANDSCAPE);
  DCHECK_EQ(print_settings->device_units_per_inch(), dpi);
  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units, true);

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  if (printer_name) {
    // Capture the system dialog settings for this printer, to be used by the
    // Print Backend service.
    base::Value::Dict dialog_data;

    dialog_data.Set(printing::kLinuxSystemPrintDialogDataPrinter, printer_name);

    ScopedGKeyFile print_settings_key_file(g_key_file_new());
    gtk_print_settings_to_key_file(settings, print_settings_key_file.get(),
                                   /*group_name=*/nullptr);

    dialog_data.Set(printing::kLinuxSystemPrintDialogDataPrintSettings,
                    g_key_file_to_data(print_settings_key_file.get(),
                                       /*length=*/nullptr, /*error=*/nullptr));

    ScopedGKeyFile page_setup_key_file(g_key_file_new());
    gtk_page_setup_to_key_file(page_setup, page_setup_key_file.get(),
                               /*group_name=*/nullptr);
    dialog_data.Set(printing::kLinuxSystemPrintDialogDataPageSetup,
                    g_key_file_to_data(page_setup_key_file.get(),
                                       /*length=*/nullptr, /*error=*/nullptr));

    print_settings->set_system_print_dialog_data(std::move(dialog_data));
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
ScopedGKeyFile::ScopedGKeyFile(GKeyFile* key_file) : key_file_(key_file) {}

ScopedGKeyFile::~ScopedGKeyFile() {
  if (key_file_) {
    g_key_file_free(key_file_.ExtractAsDangling());
  }
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

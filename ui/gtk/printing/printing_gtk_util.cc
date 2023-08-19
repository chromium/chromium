// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/printing/printing_gtk_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "printing/print_settings.h"
#include "printing/printing_context_linux.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gtk/gtk_compat.h"

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
}

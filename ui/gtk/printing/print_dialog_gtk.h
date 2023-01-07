// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_PRINTING_PRINT_DIALOG_GTK_H_
#define UI_GTK_PRINTING_PRINT_DIALOG_GTK_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "printing/print_dialog_linux_interface.h"
#include "printing/printing_context_linux.h"
#include "ui/aura/window_observer.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/gtk/gtk_compat.h"

namespace printing {
class MetafilePlayer;
class PrintSettings;
}  // namespace printing

using printing::PrintingContextLinux;

// Needs to be freed on the UI thread to clean up its GTK members variables.
class PrintDialogGtk : public printing::PrintDialogLinuxInterface,
                       public base::RefCountedDeleteOnSequence<PrintDialogGtk>,
                       public aura::WindowObserver {
 public:
  // Creates and returns a print dialog.
  static printing::PrintDialogLinuxInterface* CreatePrintDialog(
      PrintingContextLinux* context);

  PrintDialogGtk(const PrintDialogGtk&) = delete;
  PrintDialogGtk& operator=(const PrintDialogGtk&) = delete;

  // printing::PrintDialogLinuxInterface implementation.
  void UseDefaultSettings() override;
  void UpdateSettings(
      std::unique_ptr<printing::PrintSettings> settings) override;
  void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      PrintingContextLinux::PrintSettingsCallback callback) override;
  void PrintDocument(const printing::MetafilePlayer& metafile,
                     const std::u16string& document_name) override;
  void ReleaseDialog() override;

  // Handles print job response.
  void OnJobCompleted(GtkPrintJob* print_job, const GError* error);

 private:
  friend class base::RefCountedDeleteOnSequence<PrintDialogGtk>;
  friend class base::DeleteHelper<PrintDialogGtk>;

  explicit PrintDialogGtk(PrintingContextLinux* context);
  ~PrintDialogGtk() override;

  // Handles dialog response.
  CHROMEG_CALLBACK_1(PrintDialogGtk, void, OnResponse, GtkWidget*, int);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const std::u16string& document_name);

  // Helper function for initializing |context_|'s PrintSettings with a given
  // |settings|.
  void InitPrintSettings(std::unique_ptr<printing::PrintSettings> settings);

  // aura::WindowObserver implementation.
  void OnWindowDestroying(aura::Window* window) override;

  // Printing dialog callback.
  PrintingContextLinux::PrintSettingsCallback callback_;
  raw_ptr<PrintingContextLinux> context_;

  // Print dialog settings. PrintDialogGtk owns |dialog_| and holds references
  // to the other objects.
  GtkWidget* dialog_ = nullptr;
  raw_ptr<GtkPrintSettings> gtk_settings_ = nullptr;
  raw_ptr<GtkPageSetup> page_setup_ = nullptr;
  raw_ptr<GtkPrinter> printer_ = nullptr;

  base::FilePath path_to_pdf_;
};

#endif  // UI_GTK_PRINTING_PRINT_DIALOG_GTK_H_

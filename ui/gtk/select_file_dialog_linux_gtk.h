// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_
#define UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gtk/gtk_util.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"

namespace gtk {

// Implementation of SelectFileDialog that shows a Gtk common dialog for
// choosing a file or folder. This acts as a modal dialog.
class SelectFileDialogLinuxGtk : public ui::SelectFileDialogLinux,
                                 public aura::WindowObserver {
 public:
  SelectFileDialogLinuxGtk(Listener* listener,
                           std::unique_ptr<ui::SelectFilePolicy> policy);

  SelectFileDialogLinuxGtk(const SelectFileDialogLinuxGtk&) = delete;
  SelectFileDialogLinuxGtk& operator=(const SelectFileDialogLinuxGtk&) = delete;

 protected:
  ~SelectFileDialogLinuxGtk() override;

  // BaseShellDialog implementation:
  bool IsRunning(gfx::NativeWindow parent_window) const override;

  // SelectFileDialog implementation.
  // |params| is unused and must be nullptr.
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;

 private:
  friend class FilePicker;

  struct DialogState {
    DialogState();
    DialogState(std::vector<ScopedGSignal> signals,
                aura::Window* parent,
                base::OnceClosure reenable_parent_events);
    DialogState(DialogState&& other);
    DialogState& operator=(DialogState&& other);
    ~DialogState();

    std::vector<ScopedGSignal> signals;

    raw_ptr<aura::Window> parent = nullptr;

    base::OnceClosure reenable_parent_events;
  };

  bool HasMultipleFileTypeChoicesImpl() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Add the filters from |file_types_| to |chooser|.
  void AddFilters(GtkFileChooser* chooser);

  // Notifies the listener that a single file was chosen.
  void FileSelected(GtkWidget* dialog, const base::FilePath& path);

  // Notifies the listener that multiple files were chosen.
  void MultiFilesSelected(GtkWidget* dialog,
                          const std::vector<base::FilePath>& files);

  // Notifies the listener that no file was chosen (the action was canceled).
  void FileNotSelected(GtkWidget* dialog);

  GtkWidget* CreateSelectFolderDialog(Type type,
                                      const std::string& title,
                                      const base::FilePath& default_path,
                                      gfx::NativeWindow parent);

  GtkWidget* CreateFileOpenDialog(const std::string& title,
                                  const base::FilePath& default_path,
                                  gfx::NativeWindow parent);

  GtkWidget* CreateMultiFileOpenDialog(const std::string& title,
                                       const base::FilePath& default_path,
                                       gfx::NativeWindow parent);

  GtkWidget* CreateSaveAsDialog(const std::string& title,
                                const base::FilePath& default_path,
                                gfx::NativeWindow parent);

  // Check whether response_id corresponds to the user cancelling/closing the
  // dialog. Used as a helper for the below callbacks.
  bool IsCancelResponse(gint response_id);

  // Common function for OnSelectSingleFileDialogResponse and
  // OnSelectSingleFolderDialogResponse.
  void SelectSingleFileHelper(GtkWidget* dialog,
                              gint response_id,
                              bool allow_folder);

  // Common function for CreateFileOpenDialog and CreateMultiFileOpenDialog.
  GtkWidget* CreateFileOpenHelper(const std::string& title,
                                  const base::FilePath& default_path,
                                  gfx::NativeWindow parent);

  // Callback for when the user responds to a Save As or Open File dialog.
  void OnSelectSingleFileDialogResponse(GtkWidget* dialog, int response_id);

  // Callback for when the user responds to a Select Folder dialog.
  void OnSelectSingleFolderDialogResponse(GtkWidget* dialog, int response_id);

  // Callback for when the user responds to a Open Multiple Files dialog.
  void OnSelectMultiFileDialogResponse(GtkWidget* dialog, int response_id);

  // Callback for when the file chooser gets destroyed.
  void OnFileChooserDestroy(GtkWidget* dialog);

  // Callback for when we update the preview for the selection. Only used on
  // GTK3.
  void OnUpdatePreview(GtkWidget* dialog);

  // Only used on GTK3 since GTK4 provides its own preview.
  // The GtkImage widget for showing previews of selected images.
  raw_ptr<GtkWidget, DanglingUntriaged> preview_ = nullptr;

  base::flat_map<GtkWidget*, DialogState> dialogs_;
};

}  // namespace gtk

#endif  // UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

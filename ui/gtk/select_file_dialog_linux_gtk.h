// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_
#define UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

#include <map>

#include "ui/base/glib/glib_signal.h"
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
  // |params| is user data we pass back via the Listener interface.
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override;

 private:
  friend class FilePicker;
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
  // Dialog is passed so we can find that |params| pointer that was passed to
  // us when we were told to show the dialog.
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

  // Removes and returns the |params| associated with |dialog| from
  // |params_map_|.
  void* PopParamsForDialog(GtkWidget* dialog);

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
  CHROMEG_CALLBACK_1(SelectFileDialogLinuxGtk,
                     void,
                     OnSelectSingleFileDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the user responds to a Select Folder dialog.
  CHROMEG_CALLBACK_1(SelectFileDialogLinuxGtk,
                     void,
                     OnSelectSingleFolderDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the user responds to a Open Multiple Files dialog.
  CHROMEG_CALLBACK_1(SelectFileDialogLinuxGtk,
                     void,
                     OnSelectMultiFileDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the file chooser gets destroyed.
  CHROMEG_CALLBACK_0(SelectFileDialogLinuxGtk,
                     void,
                     OnFileChooserDestroy,
                     GtkWidget*);

  // Callback for when we update the preview for the selection. Only used on
  // GTK3.
  CHROMEG_CALLBACK_0(SelectFileDialogLinuxGtk,
                     void,
                     OnUpdatePreview,
                     GtkWidget*);

  // A map from dialog windows to the |params| user data associated with them.
  std::map<GtkWidget*, void*> params_map_;

  // Only used on GTK3 since GTK4 provides its own preview.
  // The GtkImage widget for showing previews of selected images.
  // This field is not a raw_ptr<> because of a static_cast not related by
  // inheritance.
  RAW_PTR_EXCLUSION GtkWidget* preview_ = nullptr;

  // Maps from dialogs to signal handler IDs.
  std::map<GtkWidget*, unsigned long> dialogs_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<aura::Window*> parents_;
};

}  // namespace gtk

#endif  // UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

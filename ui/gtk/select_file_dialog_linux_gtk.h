// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_
#define UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gtk/gtk_util.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"

namespace gtk {

// Implementation of SelectFileDialog that shows a Gtk common dialog for
// choosing a file or folder. This acts as a modal dialog.
class COMPONENT_EXPORT(GTK) SelectFileDialogLinuxGtk
    : public ui::SelectFileDialogLinux,
      public aura::WindowObserver {
 public:
  SelectFileDialogLinuxGtk(Listener* listener,
                           std::unique_ptr<ui::SelectFilePolicy> policy,
                           GtkUiPlatform* platform);

  SelectFileDialogLinuxGtk(const SelectFileDialogLinuxGtk&) = delete;
  SelectFileDialogLinuxGtk& operator=(const SelectFileDialogLinuxGtk&) = delete;

  static void EnsureActivationKeyTracking();
  static void SetActivationKeyDownForTesting(bool is_down);
  static void ResetForTesting();
  static void DispatchKeyEventForTesting(ui::KeyEvent* event);
  static void DispatchCancelModeForTesting();

 protected:
  ~SelectFileDialogLinuxGtk() override;

  // BaseShellDialog implementation:
  bool IsRunning(gfx::NativeWindow parent_window) const override;

  // SelectFileDialog implementation.
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
  friend class SelectFileDialogLinuxGtkTest;

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

    raw_ptr<GtkWidget> preview_widget = nullptr;

    base::FilePath preview_file_path;

    base::TimeTicks show_time;
  };

  struct PendingDialog {
    Type type;
    std::u16string title;
    base::FilePath default_path;
    std::optional<FileTypeInfo> file_types;
    int file_type_index;
    base::FilePath::StringType default_extension;
    raw_ptr<aura::Window> owning_window = nullptr;
  };

  bool HasMultipleFileTypeChoicesImpl() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Shows the dialog after any pending activation key release.
  void ShowPendingDialog();

  // Checks whether the dialog has been displayed for at least the minimum
  // duration before allowing an accept response.
  bool IsAcceptResponseAllowed(GtkWidget* dialog, int response_id);

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

  // Callback for when the preview file is read from disk on a background
  // thread. Only used on GTK3.
  void OnPreviewFileRead(const base::FilePath& file_path,
                         ScopedGObject<GtkWidget> chooser,
                         std::optional<std::vector<uint8_t>> bytes);

  // Callback for when the preview image is decoded out-of-process. Only used
  // on GTK3.
  void OnPreviewImageDecoded(const base::FilePath& file_path,
                             ScopedGObject<GtkWidget> chooser,
                             const SkBitmap& bitmap);

  // Callback for when the preview image is resized on a background thread.
  // Only used on GTK3.
  void OnPreviewImageResized(const base::FilePath& file_path,
                             ScopedGObject<GtkWidget> chooser,
                             const SkBitmap& bitmap);

  const raw_ptr<GtkUiPlatform> platform_;

  base::flat_map<GtkWidget*, DialogState> dialogs_;

  std::optional<PendingDialog> pending_dialog_;

  base::WeakPtrFactory<SelectFileDialogLinuxGtk> weak_factory_{this};
};

}  // namespace gtk

#endif  // UI_GTK_SELECT_FILE_DIALOG_LINUX_GTK_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/base_shell_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

class GURL;

namespace ui {

class SelectFileDialogFactory;
class SelectFilePolicy;
struct SelectedFileInfo;

// Shows a dialog box for selecting a file or a folder.
class SHELL_DIALOGS_EXPORT SelectFileDialog
    : public base::RefCountedThreadSafe<SelectFileDialog>,
      public BaseShellDialog {
 public:
  enum Type {
    SELECT_NONE,

    // For opening a folder.
    SELECT_FOLDER,

    // Like SELECT_FOLDER, but the dialog UI should explicitly show it's
    // specifically for "upload".
    SELECT_UPLOAD_FOLDER,

    // Like SELECT_FOLDER, but folder creation is disabled, if possible.
    SELECT_EXISTING_FOLDER,

    // For saving into a file, allowing a nonexistent file to be selected.
    SELECT_SAVEAS_FILE,

    // For opening a file.
    SELECT_OPEN_FILE,

    // Like SELECT_OPEN_FILE, but allowing multiple files to open.
    SELECT_OPEN_MULTI_FILE
  };

  // An interface implemented by a Listener object wishing to know about the
  // the result of the Select File/Folder action. These callbacks must be
  // re-entrant.
  class SHELL_DIALOGS_EXPORT Listener {
   public:
    // Notifies the Listener that a file/folder selection has been made. The
    // file/folder path is in |path|. |params| is contextual passed to
    // SelectFile. |index| specifies the index of the filter passed to the
    // the initial call to SelectFile.
    virtual void FileSelected(const base::FilePath& path,
                              int index,
                              void* params) = 0;

    // Similar to FileSelected() but takes SelectedFileInfo instead of
    // base::FilePath. Used for passing extra information (ex. display name).
    //
    // If not overridden, calls FileSelected() with path from |file|.
    virtual void FileSelectedWithExtraInfo(const SelectedFileInfo& file,
                                           int index,
                                           void* params);

    // Notifies the Listener that many files have been selected. The
    // files are in |files|. |params| is contextual passed to SelectFile.
    virtual void MultiFilesSelected(const std::vector<base::FilePath>& files,
                                    void* params) {}

    // Similar to MultiFilesSelected() but takes SelectedFileInfo instead of
    // base::FilePath. Used for passing extra information (ex. display name).
    //
    // If not overridden, calls MultiFilesSelected() with paths from |files|.
    virtual void MultiFilesSelectedWithExtraInfo(
        const std::vector<SelectedFileInfo>& files,
        void* params);

    // Notifies the Listener that the file/folder selection was aborted (via
    // the  user canceling or closing the selection dialog box, for example).
    // |params| is contextual passed to SelectFile.
    virtual void FileSelectionCanceled(void* params) {}

   protected:
    virtual ~Listener() = default;
  };

  // Sets the factory that creates SelectFileDialog objects, overriding default
  // behaviour.
  //
  // This is optional and should only be used by components that have to live
  // elsewhere in the tree due to layering violations. (For example, because of
  // a dependency on chrome's extension system.) Takes ownership of `factory`,
  // destroying it on the next SetFactory() call, and leaking otherwise.
  static void SetFactory(std::unique_ptr<SelectFileDialogFactory> factory);

  // Creates a dialog box helper. This is an inexpensive wrapper around the
  // platform-native file selection dialog. |policy| is an optional class that
  // can prevent showing a dialog.
  //
  // The lifetime of the Listener is not managed by this class. The calling
  // code should call always ListenerDestroyed() (on the base class
  // BaseShellDialog) when the listener is destroyed since the SelectFileDialog
  // is refcounted and uses a background thread.
  static scoped_refptr<SelectFileDialog> Create(
      Listener* listener,
      std::unique_ptr<SelectFilePolicy> policy);

  SelectFileDialog(const SelectFileDialog&) = delete;
  SelectFileDialog& operator=(const SelectFileDialog&) = delete;

  // Holds information about allowed extensions on a file save dialog.
  struct SHELL_DIALOGS_EXPORT FileTypeInfo {
    FileTypeInfo();
    FileTypeInfo(const FileTypeInfo& other);
    ~FileTypeInfo();

    // A list of allowed extensions. For example, it might be
    //
    //   { { "htm", "html" }, { "txt" } }
    //
    // Only pass more than one extension in the inner vector if the extensions
    // are equivalent. Do NOT include leading periods.
    std::vector<std::vector<base::FilePath::StringType>> extensions;

    // Overrides the system descriptions of the specified extensions. Entries
    // correspond to |extensions|; if left blank the system descriptions will
    // be used.
    std::vector<std::u16string> extension_description_overrides;

    // Specifies whether there will be a filter added for all files (i.e. *.*).
    bool include_all_files = false;

    // Some implementations by default hide the extension of a file, in
    // particular in a save file dialog. If this is set to true, where
    // supported, the save file dialog will instead keep the file extension
    // visible.
    bool keep_extension_visible = false;

    // Specifies which type of paths the caller can handle.
    enum AllowedPaths {
      // Any type of path, whether on a local/native volume or a remote/virtual
      // volume. Excludes files that can only be opened by URL; for those use
      // ANY_PATH_OR_URL below.
      ANY_PATH,
      // Set when the caller cannot handle virtual volumes (e.g. File System
      // Provider [FSP] volumes like "File System for Dropbox"). When opening
      // files, the dialog will create a native replica of the file and return
      // its path. When saving files, the dialog will hide virtual volumes.
      NATIVE_PATH,
      // Set when the caller can open files via URL. For example, when opening a
      // .gdoc file from Google Drive the file is opened by navigating to a
      // docs.google.com URL.
      ANY_PATH_OR_URL
    };
    AllowedPaths allowed_paths = NATIVE_PATH;
  };

  // Returns a file path with a base name at most 255 characters long. This
  // is the limit on Windows and Linux, and on Windows the system file
  // selection dialog will fail to open if the file name exceeds 255 characters.
  static base::FilePath GetShortenedFilePath(const base::FilePath& path);

  // Selects a File.
  // Before doing anything this function checks if FileBrowsing is forbidden
  // by Policy. If so, it tries to show an InfoBar and behaves as though no File
  // was selected (the user clicked `Cancel` immediately).
  // Otherwise it will start displaying the dialog box. This will also
  // block the calling window until the dialog box is complete. The listener
  // associated with this object will be notified when the selection is
  // complete.
  // |type| is the type of file dialog to be shown, see Type enumeration above.
  // |title| is the title to be displayed in the dialog. If this string is
  //   empty, the default title is used.
  // |default_path| is the default path and suggested file name to be shown in
  //   the dialog. This only works for SELECT_SAVEAS_FILE and SELECT_OPEN_FILE.
  //   Can be an empty string to indicate the platform default.
  // |file_types| holds the information about the file types allowed. Pass NULL
  //   to get no special behavior
  // |file_type_index| is the 1-based index into the file type list in
  //   |file_types|. Specify 0 if you don't need to specify extension behavior.
  // |default_extension| is the default extension to add to the file if the
  //   user doesn't type one. This should NOT include the '.'. On Windows, if
  //   you specify this you must also specify |file_types|.
  // |owning_window| is the window the dialog is modal to, or NULL for a
  //   modeless dialog.
  // |params| is data from the calling context which will be passed through to
  //   the listener. Can be NULL.
  // |caller| is the URL of the dialog caller which can be used to check further
  // Policy restrictions, when applicable. Can be NULL.
  // NOTE: only one instance of any shell dialog can be shown per owning_window
  // at a time (for obvious reasons).
  void SelectFile(Type type,
                  const std::u16string& title,
                  const base::FilePath& default_path,
                  const FileTypeInfo* file_types,
                  int file_type_index,
                  const base::FilePath::StringType& default_extension,
                  gfx::NativeWindow owning_window,
                  void* params,
                  const GURL* caller = nullptr);
  bool HasMultipleFileTypeChoices();

 protected:
  friend class base::RefCountedThreadSafe<SelectFileDialog>;

  explicit SelectFileDialog(Listener* listener,
                            std::unique_ptr<SelectFilePolicy> policy);
  ~SelectFileDialog() override;

  // Displays the actual file-selection dialog.
  // This is overridden in the platform-specific descendants of FileSelectDialog
  // and gets called from SelectFile after testing the
  // AllowFileSelectionDialogs-Policy.
  virtual void SelectFileImpl(
      Type type,
      const std::u16string& title,
      const base::FilePath& default_path,
      const FileTypeInfo* file_types,
      int file_type_index,
      const base::FilePath::StringType& default_extension,
      gfx::NativeWindow owning_window,
      void* params,
      const GURL* caller) = 0;

  // The listener to be notified of selection completion.
  raw_ptr<Listener, DanglingUntriaged> listener_;

 private:
  // Tests if the file selection dialog can be displayed by
  // testing if the AllowFileSelectionDialogs-Policy is
  // either unset or set to true.
  bool CanOpenSelectFileDialog();

  // Informs the |listener_| that the file selection dialog was canceled. Moved
  // to a function for being able to post it to the message loop.
  void CancelFileSelection(void* params);

  // Returns true if the dialog has multiple file type choices.
  virtual bool HasMultipleFileTypeChoicesImpl() = 0;

  std::unique_ptr<SelectFilePolicy> select_file_policy_;
};

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_H_

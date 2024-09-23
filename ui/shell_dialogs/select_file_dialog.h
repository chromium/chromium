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
#include "build/build_config.h"
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
  // the result of the Select File/Folder action. For any given call to
  // SelectFile(), exactly one of these callbacks will be invoked once, possibly
  // *but not necessarily* while SelectFile() is still on the stack.
  class SHELL_DIALOGS_EXPORT Listener {
   public:
    // Notifies the Listener that a file/folder selection has been made. The
    // file/folder path is in |file|. |index| specifies the index of the filter
    // passed to the initial call to SelectFile.
    virtual void FileSelected(const SelectedFileInfo& file, int index) {}

    // Notifies the Listener that many files have been selected. The files are
    // in |files|.
    // Implementing this method is optional if no multi-file selection is ever
    // made, as the default implementation will call NOTREACHED.
    virtual void MultiFilesSelected(const std::vector<SelectedFileInfo>& files);

    // Notifies the Listener that the file/folder selection was canceled (via
    // the user canceling or closing the selection dialog box, for example).
    virtual void FileSelectionCanceled() {}

   protected:
    virtual ~Listener() = default;
  };

  // Sets the factory that creates SelectFileDialog objects, overriding default
  // behaviour.
  //
  // This is optional and should only be used by components that have to live
  // elsewhere in the tree due to layering violations. (For example, because of
  // a dependency on chrome's extension system.)
  static void SetFactory(std::unique_ptr<SelectFileDialogFactory> factory);

  // Creates a dialog box helper. This is an inexpensive wrapper around the
  // platform-native file selection dialog. |policy| is an optional class that
  // can prevent showing a dialog.
  //
  // Note that Listener's lifetime is tricky to get right: Listener must outlive
  // the returned SelectFileDialog, but there may be hidden references to that
  // object held by worker threads. In general it is only safe to destroy the
  // listener when there are no calls to SelectFile() outstanding.
  static scoped_refptr<SelectFileDialog> Create(
      Listener* listener,
      std::unique_ptr<SelectFilePolicy> policy);

  SelectFileDialog(const SelectFileDialog&) = delete;
  SelectFileDialog& operator=(const SelectFileDialog&) = delete;

  // Holds information about allowed extensions on a file save dialog.
  struct SHELL_DIALOGS_EXPORT FileTypeInfo {
    using FileExtensionList = std::vector<base::FilePath::StringType>;

    FileTypeInfo();
    FileTypeInfo(const FileTypeInfo& other);

    explicit FileTypeInfo(FileExtensionList extensions);
    explicit FileTypeInfo(std::vector<FileExtensionList> extensions);
    explicit FileTypeInfo(std::vector<FileExtensionList> extensions,
                          std::vector<std::u16string> descriptions);

    ~FileTypeInfo();

    // A list of allowed extensions. For example, it might be
    //
    //   { { "htm", "html" }, { "txt" } }
    //
    // Only pass more than one extension in the inner vector if the extensions
    // are equivalent. Do NOT include leading periods.
    std::vector<std::vector<base::FilePath::StringType>> extensions;

    // Overrides the system descriptions of the specified extensions. Entries
    // correspond to |extensions|. This must either be of length 0 or the same
    // length as extensions.
    // TODO(https://issues.chromium.org/issues/340178601): store a vector of
    // FileTypeExtensions instead of this and the above vector?
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

#if BUILDFLAG(IS_ANDROID)
  // Set the list of acceptable MIME types for the file picker; this will apply
  // to any subsequent SelectFile() calls.
  virtual void SetAcceptTypes(std::vector<std::u16string> types);

  // Set whether the media picker should try to use media capture, meaning
  // offering whatever means the system has of recording media (audio, video,
  // etc) as a "file" choice.
  virtual void SetUseMediaCapture(bool use_media_capture);

  // Set whether files should be opened as writable using the
  // ACTION_OPEN_DOCUMENT Intent rather than ACTION_GET_CONTENT.
  virtual void SetOpenWritable(bool open_writable);
#endif

  // Selects a File.
  // Before doing anything this function checks if FileBrowsing is forbidden
  // by the SelectFilePolicy supplied at creation time. If so, it tries to show
  // an InfoBar and behaves as though the dialog was cancelled, by posting a
  // call back to the listener's FileSelectionCanceled method. Otherwise, it
  // starts showing the dialog.
  //
  // On some platforms (Linux-kde), this method blocks the entire UI thread
  // until the dialog is closed. On others (Mac) it may enter a nested message
  // loop. It sometimes, but not always, tries to block input events from being
  // delivered to its owning window (Windows, Linux-gtk). On some platforms it
  // uses a background thread, although listener callbacks are always run on the
  // UI thread. In general when using this method you must ensure that the
  // Listener, and any data passed to SelectFile(), remain live until one of the
  // callbacks has been run.
  //
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
  // |caller| is the URL of the dialog caller which can be used to check further
  //   policy restrictions, when applicable. Can be NULL. Non-NULL values of
  //   |caller| are deprecated - store the state in the calling object directly.
  // NOTE: only one instance of any shell dialog can be shown per owning_window
  // at a time (for obvious reasons).
  void SelectFile(Type type,
                  const std::u16string& title,
                  const base::FilePath& default_path,
                  const FileTypeInfo* file_types,
                  int file_type_index,
                  const base::FilePath::StringType& default_extension,
                  gfx::NativeWindow owning_window,
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
      const GURL* caller) = 0;

  // The listener to be notified of selection completion.
  raw_ptr<Listener> listener_;

 private:
  // Tests if the file selection dialog can be displayed by
  // testing if the AllowFileSelectionDialogs-Policy is
  // either unset or set to true.
  bool CanOpenSelectFileDialog();

  // Informs the |listener_| that the file selection dialog was canceled. Moved
  // to a function for being able to post it to the message loop.
  void CancelFileSelection();

  // Returns true if the dialog has multiple file type choices.
  virtual bool HasMultipleFileTypeChoicesImpl() = 0;

  std::unique_ptr<SelectFilePolicy> select_file_policy_;
};

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_H_

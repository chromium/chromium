// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_win.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/registry.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/shell_dialogs/execute_select_file_win.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/select_file_utils_win.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace ui {

namespace {

// Get the file type description from the registry. This will be "Text Document"
// for .txt files, "JPEG Image" for .jpg files, etc. If the registry doesn't
// have an entry for the file type, we return false, true if the description was
// found. 'file_ext' must be in form ".txt".
bool GetRegistryDescriptionFromExtension(const std::u16string& file_ext,
                                         std::u16string* reg_description) {
  DCHECK(reg_description);
  base::win::RegKey reg_ext(HKEY_CLASSES_ROOT, base::as_wcstr(file_ext),
                            KEY_READ);
  std::wstring reg_app;
  if (reg_ext.ReadValue(nullptr, &reg_app) == ERROR_SUCCESS &&
      !reg_app.empty()) {
    base::win::RegKey reg_link(HKEY_CLASSES_ROOT, reg_app.c_str(), KEY_READ);
    std::wstring description;
    if (reg_link.ReadValue(nullptr, &description) == ERROR_SUCCESS) {
      *reg_description = base::WideToUTF16(description);
      return true;
    }
  }
  return false;
}

// Set up a filter for a Save/Open dialog, |ext_desc| as the text descriptions
// of the |file_ext| types (optional), and (optionally) the default 'All Files'
// view. The purpose of the filter is to show only files of a particular type in
// a Windows Save/Open dialog box. The resulting filter is returned. The filter
// created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, a
// default description will be created (e.g. "qqq" yields "QQQ File").
std::vector<FileFilterSpec> FormatFilterForExtensions(
    const std::vector<std::u16string>& file_ext,
    const std::vector<std::u16string>& ext_desc,
    bool include_all_files,
    bool keep_extension_visible) {
  const std::u16string all_ext = u"*.*";
  const std::u16string all_desc =
      l10n_util::GetStringUTF16(IDS_APP_SAVEAS_ALL_FILES);

  DCHECK(file_ext.size() >= ext_desc.size());

  if (file_ext.empty())
    include_all_files = true;

  std::vector<FileFilterSpec> result;
  result.reserve(file_ext.size() + 1);

  for (size_t i = 0; i < file_ext.size(); ++i) {
    std::u16string ext =
        RemoveEnvVarFromFileName<char16_t>(file_ext[i], std::u16string(u"%"));
    std::u16string desc;
    if (i < ext_desc.size())
      desc = ext_desc[i];

    if (ext.empty()) {
      // Force something reasonable to appear in the dialog box if there is no
      // extension provided.
      include_all_files = true;
      continue;
    }

    if (desc.empty()) {
      DCHECK(ext.find(u'.') != std::u16string::npos);
      std::u16string first_extension = ext.substr(ext.find(u'.'));
      size_t first_separator_index = first_extension.find(u';');
      if (first_separator_index != std::u16string::npos)
        first_extension = first_extension.substr(0, first_separator_index);

      // Find the extension name without the preceeding '.' character.
      std::u16string ext_name = first_extension;
      size_t ext_index = ext_name.find_first_not_of(u'.');
      if (ext_index != std::u16string::npos)
        ext_name = ext_name.substr(ext_index);

      if (!GetRegistryDescriptionFromExtension(first_extension, &desc)) {
        // The extension doesn't exist in the registry. Create a description
        // based on the unknown extension type (i.e. if the extension is .qqq,
        // then we create a description "QQQ File").
        desc = l10n_util::GetStringFUTF16(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                          base::i18n::ToUpper(ext_name));
        include_all_files = true;
      }
      if (desc.empty())
        desc = u"*." + ext_name;
    } else if (keep_extension_visible) {
      // Having '*' in the description could cause the windows file dialog to
      // not include the file extension in the file dialog. So strip out any '*'
      // characters if `keep_extension_visible` is set.
      base::ReplaceChars(desc, u"*", std::u16string_view(), &desc);
    }

    result.push_back({desc, ext});
  }

  if (include_all_files)
    result.push_back({all_desc, all_ext});

  return result;
}

// Forwards the result from a select file operation to the SelectFileDialog
// object on the UI thread.
void OnSelectFileExecutedOnDialogTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    OnSelectFileExecutedCallback on_select_file_executed_callback,
    const std::vector<base::FilePath>& paths,
    int index) {
  ui_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_select_file_executed_callback),
                                paths, index));
}

// Implementation of SelectFileDialog that shows a Windows common dialog for
// choosing a file or folder.
class SelectFileDialogImpl : public ui::SelectFileDialog,
                             public ui::BaseShellDialogImpl {
 public:
  SelectFileDialogImpl(
      Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy,
      const ExecuteSelectFileCallback& execute_select_file_callback);

  SelectFileDialogImpl(const SelectFileDialogImpl&) = delete;
  SelectFileDialogImpl& operator=(const SelectFileDialogImpl&) = delete;

  // BaseShellDialog implementation:
  bool IsRunning(gfx::NativeWindow owning_window) const override;
  void ListenerDestroyed() override;

 protected:
  // SelectFileDialog implementation:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;

 private:
  ~SelectFileDialogImpl() override;

  struct SelectFolderDialogOptions {
    const wchar_t* default_path;
    bool is_upload;
  };

  // Returns the result of the select file operation to the listener.
  void OnSelectFileExecuted(Type type,
                            std::unique_ptr<RunState> run_state,
                            const std::vector<base::FilePath>& paths,
                            int index);

  bool HasMultipleFileTypeChoicesImpl() override;

  // Returns the filter to be used while displaying the open/save file dialog.
  // This is computed from the extensions for the file types being opened.
  // |file_types| can be NULL in which case the returned filter will be empty.
  static std::vector<FileFilterSpec> GetFilterForFileTypes(
      const FileTypeInfo* file_types);

  bool has_multiple_file_type_choices_;
  ExecuteSelectFileCallback execute_select_file_callback_;
};

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    const ExecuteSelectFileCallback& execute_select_file_callback)
    : SelectFileDialog(listener, std::move(policy)),
      BaseShellDialogImpl(),
      has_multiple_file_type_choices_(false),
      execute_select_file_callback_(execute_select_file_callback) {}

SelectFileDialogImpl::~SelectFileDialogImpl() = default;

// Invokes the |execute_select_file_callback| and returns the result to
void DoSelectFileOnDialogTaskRunner(
    const ExecuteSelectFileCallback& execute_select_file_callback,
    SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const std::wstring& default_extension,
    HWND owner,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    OnSelectFileExecutedCallback on_select_file_executed_callback) {
  execute_select_file_callback.Run(
      type, title, default_path, filter, file_type_index, default_extension,
      owner,
      base::BindOnce(&OnSelectFileExecutedOnDialogTaskRunner,
                     std::move(ui_task_runner),
                     std::move(on_select_file_executed_callback)));
}

void SelectFileDialogImpl::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    const GURL* caller) {
  has_multiple_file_type_choices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  std::vector<FileFilterSpec> filter = GetFilterForFileTypes(file_types);
  HWND owner = owning_window && owning_window->GetRootWindow()
                   ? owning_window->GetHost()->GetAcceleratedWidget()
                   : nullptr;

  std::unique_ptr<RunState> run_state = BeginRun(owner);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      run_state->dialog_task_runner;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoSelectFileOnDialogTaskRunner,
                     execute_select_file_callback_, type, title, default_path,
                     filter, file_type_index, default_extension, owner,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&SelectFileDialogImpl::OnSelectFileExecuted,
                                    this, type, std::move(run_state))));
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow owning_window) const {
  if (!owning_window->GetRootWindow())
    return false;
  HWND owner = owning_window->GetHost()->GetAcceleratedWidget();
  return listener_ && IsRunningDialogForOwner(owner);
}

void SelectFileDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = nullptr;
}

void SelectFileDialogImpl::OnSelectFileExecuted(
    Type type,
    std::unique_ptr<RunState> run_state,
    const std::vector<base::FilePath>& paths,
    int index) {
  if (listener_) {
    // The paths vector is empty when the user cancels the dialog.
    if (paths.empty()) {
      listener_->FileSelectionCanceled();
    } else {
      switch (type) {
        case SELECT_FOLDER:
        case SELECT_UPLOAD_FOLDER:
        case SELECT_EXISTING_FOLDER:
        case SELECT_SAVEAS_FILE:
        case SELECT_OPEN_FILE:
          DCHECK_EQ(paths.size(), 1u);
          listener_->FileSelected(SelectedFileInfo(paths[0]), index);
          break;
        case SELECT_OPEN_MULTI_FILE:
          listener_->MultiFilesSelected(
              FilePathListToSelectedFileInfoList(paths));
          break;
        case SELECT_NONE:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

  EndRun(std::move(run_state));
}

// static
std::vector<FileFilterSpec> SelectFileDialogImpl::GetFilterForFileTypes(
    const FileTypeInfo* file_types) {
  if (!file_types)
    return std::vector<FileFilterSpec>();

  std::vector<std::u16string> exts;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<std::wstring>& inner_exts = file_types->extensions[i];
    std::u16string ext_string;
    for (size_t j = 0; j < inner_exts.size(); ++j) {
      if (!ext_string.empty())
        ext_string.push_back(u';');
      ext_string.append(u"*.");
      ext_string.append(base::WideToUTF16(inner_exts[j]));
    }
    exts.push_back(ext_string);
  }
  return FormatFilterForExtensions(
      exts, file_types->extension_description_overrides,
      file_types->include_all_files, file_types->keep_extension_visible);
}

}  // namespace

SelectFileDialog* CreateWinSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy,
    const ExecuteSelectFileCallback& execute_select_file_callback) {
  return new SelectFileDialogImpl(listener, std::move(policy),
                                  execute_select_file_callback);
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return CreateWinSelectFileDialog(listener, std::move(policy),
                                   base::BindRepeating(&ui::ExecuteSelectFile));
}

}  // namespace ui

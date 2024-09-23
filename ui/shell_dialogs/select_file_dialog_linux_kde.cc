// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <set>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/nix/xdg_util.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace {

std::string GetTitle(const std::string& title, int message_id) {
  return title.empty() ? l10n_util::GetStringUTF8(message_id) : title;
}

const char kKdialogBinary[] = "kdialog";

}  // namespace

namespace ui {

// Implementation of SelectFileDialog that shows a KDE common dialog for
// choosing a file or folder. This acts as a modal dialog.
class SelectFileDialogLinuxKde : public SelectFileDialogLinux {
 public:
  SelectFileDialogLinuxKde(Listener* listener,
                           std::unique_ptr<ui::SelectFilePolicy> policy,
                           base::nix::DesktopEnvironment desktop,
                           const std::string& kdialog_version);

  SelectFileDialogLinuxKde(const SelectFileDialogLinuxKde&) = delete;
  SelectFileDialogLinuxKde& operator=(const SelectFileDialogLinuxKde&) = delete;

 protected:
  ~SelectFileDialogLinuxKde() override;

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
  bool HasMultipleFileTypeChoicesImpl() override;

  struct KDialogParams {
    KDialogParams(const std::string& type,
                  const std::string& title,
                  const base::FilePath& default_path,
                  gfx::AcceleratedWidget parent,
                  bool file_operation,
                  bool multiple_selection)
        : type(type),
          title(title),
          default_path(default_path),
          parent(parent),
          file_operation(file_operation),
          multiple_selection(multiple_selection) {}

    std::string type;
    std::string title;
    base::FilePath default_path;
    gfx::AcceleratedWidget parent;
    bool file_operation;
    bool multiple_selection;
  };

  struct KDialogOutputParams {
    std::string output;
    int exit_code;
  };

  // Get the filters from |file_types_| and concatenate them into
  // |filter_string|.
  std::string GetMimeTypeFilterString();

  // Get KDialog command line representing the Argv array for KDialog.
  void GetKDialogCommandLine(const std::string& type,
                             const std::string& title,
                             const base::FilePath& default_path,
                             gfx::AcceleratedWidget parent,
                             bool file_operation,
                             bool multiple_selection,
                             base::CommandLine* command_line);

  // Call KDialog on the FILE thread and return the results.
  std::unique_ptr<KDialogOutputParams> CallKDialogOutput(
      const KDialogParams& params);

  // Notifies the listener that a single file was chosen.
  void FileSelected(const base::FilePath& path);

  // Notifies the listener that multiple files were chosen.
  void MultiFilesSelected(const std::vector<base::FilePath>& files);

  // Notifies the listener that no file was chosen (the action was canceled).
  void FileNotSelected();

  void CreateSelectFolderDialog(Type type,
                                const std::string& title,
                                const base::FilePath& default_path,
                                gfx::AcceleratedWidget parent);

  void CreateFileOpenDialog(const std::string& title,
                            const base::FilePath& default_path,
                            gfx::AcceleratedWidget parent);

  void CreateMultiFileOpenDialog(const std::string& title,
                                 const base::FilePath& default_path,
                                 gfx::AcceleratedWidget parent);

  void CreateSaveAsDialog(const std::string& title,
                          const base::FilePath& default_path,
                          gfx::AcceleratedWidget parent);

  // Common function for OnSelectSingleFileDialogResponse and
  // OnSelectSingleFolderDialogResponse.
  void SelectSingleFileHelper(bool allow_folder,
                              std::unique_ptr<KDialogOutputParams> results);

  void OnSelectSingleFileDialogResponse(
      gfx::AcceleratedWidget parent,
      std::unique_ptr<KDialogOutputParams> results);
  void OnSelectMultiFileDialogResponse(
      gfx::AcceleratedWidget parent,
      std::unique_ptr<KDialogOutputParams> results);
  void OnSelectSingleFolderDialogResponse(
      gfx::AcceleratedWidget parent,
      std::unique_ptr<KDialogOutputParams> results);

  // Should be either DESKTOP_ENVIRONMENT_KDE3, KDE4, KDE5, or KDE6.
  base::nix::DesktopEnvironment desktop_;

  // The set of all parent windows for which we are currently running
  // dialogs. This should only be accessed on the UI thread.
  std::set<gfx::AcceleratedWidget> parents_;

  // Set to true if the kdialog version is new enough to support passing
  // multiple extensions with descriptions, eliminating the need for the lossy
  // conversion of extensions to mime-types.
  bool kdialog_supports_multiple_extensions_ = false;

  // A task runner for blocking pipe reads.
  scoped_refptr<base::SequencedTaskRunner> pipe_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
bool SelectFileDialogLinux::CheckKDEDialogWorksOnUIThread(
    std::string& kdialog_version) {
  // No choice. UI thread can't continue without an answer here. Fortunately we
  // only do this once, the first time a file dialog is displayed.
  base::ScopedAllowBlocking scoped_allow_blocking;

  base::CommandLine::StringVector cmd_vector;
  cmd_vector.push_back(kKdialogBinary);
  cmd_vector.push_back("--version");
  base::CommandLine command_line(cmd_vector);
  return base::GetAppOutput(command_line, &kdialog_version);
}

SelectFileDialog* NewSelectFileDialogLinuxKde(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    base::nix::DesktopEnvironment desktop,
    const std::string& kdialog_version) {
  return new SelectFileDialogLinuxKde(listener, std::move(policy), desktop,
                                      kdialog_version);
}

SelectFileDialogLinuxKde::SelectFileDialogLinuxKde(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    base::nix::DesktopEnvironment desktop,
    const std::string& kdialog_version)
    : SelectFileDialogLinux(listener, std::move(policy)),
      desktop_(desktop),
      pipe_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(desktop_ == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
         desktop_ == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
         desktop_ == base::nix::DESKTOP_ENVIRONMENT_KDE5 ||
         desktop_ == base::nix::DESKTOP_ENVIRONMENT_KDE6);
  // |kdialog_version| should be of the form "kdialog 1.2.3", so split on
  // whitespace and then try to parse a version from the second piece. If
  // parsing fails for whatever reason, we fall back to the behavior that works
  // with all currently known versions of kdialog.
  std::vector<std::string_view> version_pieces = base::SplitStringPiece(
      kdialog_version, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (version_pieces.size() >= 2) {
    base::Version parsed_version(version_pieces[1]);
    if (parsed_version.IsValid()) {
      kdialog_supports_multiple_extensions_ =
          parsed_version >= base::Version("19.12");
    }
  }
}

SelectFileDialogLinuxKde::~SelectFileDialogLinuxKde() = default;

bool SelectFileDialogLinuxKde::IsRunning(
    gfx::NativeWindow parent_window) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_window && parent_window->GetHost()) {
    auto window = parent_window->GetHost()->GetAcceleratedWidget();
    return parents_.find(window) != parents_.end();
  }

  return false;
}

// We ignore |default_extension|.
void SelectFileDialogLinuxKde::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    const GURL* caller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_type(type);

  gfx::AcceleratedWidget window = gfx::kNullAcceleratedWidget;
  if (owning_window && owning_window->GetHost()) {
    // |owning_window| can be null when user right-clicks on a downloadable item
    // and chooses 'Open Link in New Tab' when 'Ask where to save each file
    // before downloading.' preference is turned on. (http://crbug.com/29213)
    window = owning_window->GetHost()->GetAcceleratedWidget();
    parents_.insert(window);
  }

  std::string title_string = base::UTF16ToUTF8(title);

  set_file_type_index(file_type_index);
  if (file_types) {
    set_file_types(*file_types);
  } else {
    auto file_types_copy = SelectFileDialogLinux::file_types();
    file_types_copy.include_all_files = true;
    set_file_types(file_types_copy);
  }

  switch (type) {
    case SELECT_FOLDER:
    case SELECT_UPLOAD_FOLDER:
    case SELECT_EXISTING_FOLDER:
      CreateSelectFolderDialog(type, title_string, default_path, window);
      return;
    case SELECT_OPEN_FILE:
      CreateFileOpenDialog(title_string, default_path, window);
      return;
    case SELECT_OPEN_MULTI_FILE:
      CreateMultiFileOpenDialog(title_string, default_path, window);
      return;
    case SELECT_SAVEAS_FILE:
      CreateSaveAsDialog(title_string, default_path, window);
      return;
    case SELECT_NONE:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

bool SelectFileDialogLinuxKde::HasMultipleFileTypeChoicesImpl() {
  return file_types().extensions.size() > 1;
}

std::string SelectFileDialogLinuxKde::GetMimeTypeFilterString() {
  DCHECK(pipe_task_runner_->RunsTasksInCurrentSequence());

  if (!kdialog_supports_multiple_extensions_) {
    // We need a filter set because the same mime type can appear multiple
    // times.
    std::set<std::string> filter_set;
    for (auto& extensions : file_types().extensions) {
      for (auto& extension : extensions) {
        if (!extension.empty()) {
          std::string mime_type = base::nix::GetFileMimeType(
              base::FilePath("name").ReplaceExtension(extension));
          filter_set.insert(mime_type);
        }
      }
    }
    std::vector<std::string> filter_vector(filter_set.cbegin(),
                                           filter_set.cend());
    // Add the *.* filter, but only if we have added other filters (otherwise it
    // is implied). It needs to be added last to avoid being picked as the
    // default filter.
    if (file_types().include_all_files && !file_types().extensions.empty()) {
      DCHECK(filter_set.find("application/octet-stream") == filter_set.end());
      filter_vector.push_back("application/octet-stream");
    }
    return base::JoinString(filter_vector, " ");
  }

  std::vector<std::string> filters;
  for (size_t i = 0; i < file_types().extensions.size(); ++i) {
    std::set<std::string> extension_filters;
    for (const auto& extension : file_types().extensions[i]) {
      if (extension.empty())
        continue;
      extension_filters.insert(std::string("*.") + extension);
    }

    // We didn't find any non-empty extensions to filter on.
    if (extension_filters.empty())
      continue;

    std::vector<std::string> extension_filters_vector(extension_filters.begin(),
                                                      extension_filters.end());

    std::string description;
    // The description vector may be blank, in which case we are supposed to
    // use some sort of default description based on the filter.
    if (i < file_types().extension_description_overrides.size()) {
      description =
          base::UTF16ToUTF8(file_types().extension_description_overrides[i]);
      // Filter out any characters that would mess up kdialog's parsing.
      base::ReplaceChars(description, "|()", "", &description);
    } else {
      // There is no system default filter description so we use
      // the extensions themselves if the description is blank.
      description = base::JoinString(extension_filters_vector, ",");
    }

    filters.push_back(description + " (" +
                      base::JoinString(extension_filters_vector, " ") + ")");
  }

  if (file_types().include_all_files && !file_types().extensions.empty())
    filters.push_back(l10n_util::GetStringUTF8(IDS_SAVEAS_ALL_FILES) + " (*)");

  return base::JoinString(filters, "|");
}

std::unique_ptr<SelectFileDialogLinuxKde::KDialogOutputParams>
SelectFileDialogLinuxKde::CallKDialogOutput(const KDialogParams& params) {
  DCHECK(pipe_task_runner_->RunsTasksInCurrentSequence());
  base::CommandLine::StringVector cmd_vector;
  cmd_vector.push_back(kKdialogBinary);
  base::CommandLine command_line(cmd_vector);
  GetKDialogCommandLine(params.type, params.title, params.default_path,
                        params.parent, params.file_operation,
                        params.multiple_selection, &command_line);

  auto results = std::make_unique<KDialogOutputParams>();
  // Get output from KDialog
  base::GetAppOutputWithExitCode(command_line, &results->output,
                                 &results->exit_code);
  if (!results->output.empty())
    results->output.erase(results->output.size() - 1);
  return results;
}

void SelectFileDialogLinuxKde::GetKDialogCommandLine(
    const std::string& type,
    const std::string& title,
    const base::FilePath& path,
    gfx::AcceleratedWidget parent,
    bool file_operation,
    bool multiple_selection,
    base::CommandLine* command_line) {
  CHECK(command_line);

  // Attach to the current Chrome window.
  if (parent != gfx::kNullAcceleratedWidget) {
    command_line->AppendSwitchNative(
        desktop_ == base::nix::DESKTOP_ENVIRONMENT_KDE3 ? "--embed"
                                                        : "--attach",
        base::NumberToString(static_cast<uint32_t>(parent)));
  }

  // Set the correct title for the dialog.
  if (!title.empty())
    command_line->AppendSwitchNative("--title", title);
  // Enable multiple file selection if we need to.
  if (multiple_selection) {
    command_line->AppendSwitch("--multiple");
    command_line->AppendSwitch("--separate-output");
  }
  command_line->AppendSwitch(type);
  // The path should never be empty. If it is, set it to PWD.
  if (path.empty())
    command_line->AppendArgPath(base::FilePath("."));
  else
    command_line->AppendArgPath(path);
  // Depending on the type of the operation we need, get the path to the
  // file/folder and set up mime type filters.
  if (file_operation)
    command_line->AppendArg(GetMimeTypeFilterString());
  VLOG(1) << "KDialog command line: " << command_line->GetCommandLineString();
}

void SelectFileDialogLinuxKde::FileSelected(const base::FilePath& path) {
  if (type() == SELECT_SAVEAS_FILE)
    set_last_saved_path(path.DirName());
  else if (type() == SELECT_OPEN_FILE)
    set_last_opened_path(path.DirName());
  else if (type() == SELECT_FOLDER || type() == SELECT_UPLOAD_FOLDER ||
           type() == SELECT_EXISTING_FOLDER)
    set_last_opened_path(path);
  else
    NOTREACHED_IN_MIGRATION();
  if (listener_) {  // What does the filter index actually do?
    // TODO(dfilimon): Get a reasonable index value from somewhere.
    listener_->FileSelected(SelectedFileInfo(path), 1);
  }
}

void SelectFileDialogLinuxKde::MultiFilesSelected(
    const std::vector<base::FilePath>& files) {
  set_last_opened_path(files[0].DirName());
  if (listener_)
    listener_->MultiFilesSelected(FilePathListToSelectedFileInfoList(files));
}

void SelectFileDialogLinuxKde::FileNotSelected() {
  if (listener_)
    listener_->FileSelectionCanceled();
}

void SelectFileDialogLinuxKde::CreateSelectFolderDialog(
    Type type,
    const std::string& title,
    const base::FilePath& default_path,
    gfx::AcceleratedWidget parent) {
  int title_message_id = (type == SELECT_UPLOAD_FOLDER)
                             ? IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE
                             : IDS_SELECT_FOLDER_DIALOG_TITLE;
  pipe_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxKde::CallKDialogOutput, this,
          KDialogParams(
              "--getexistingdirectory", GetTitle(title, title_message_id),
              default_path.empty() ? *last_opened_path() : default_path, parent,
              false, false)),
      base::BindOnce(
          &SelectFileDialogLinuxKde::OnSelectSingleFolderDialogResponse, this,
          parent));
}

void SelectFileDialogLinuxKde::CreateFileOpenDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::AcceleratedWidget parent) {
  pipe_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxKde::CallKDialogOutput, this,
          KDialogParams(
              "--getopenfilename", GetTitle(title, IDS_OPEN_FILE_DIALOG_TITLE),
              default_path.empty() ? *last_opened_path() : default_path, parent,
              true, false)),
      base::BindOnce(
          &SelectFileDialogLinuxKde::OnSelectSingleFileDialogResponse, this,
          parent));
}

void SelectFileDialogLinuxKde::CreateMultiFileOpenDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::AcceleratedWidget parent) {
  pipe_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxKde::CallKDialogOutput, this,
          KDialogParams(
              "--getopenfilename", GetTitle(title, IDS_OPEN_FILES_DIALOG_TITLE),
              default_path.empty() ? *last_opened_path() : default_path, parent,
              true, true)),
      base::BindOnce(&SelectFileDialogLinuxKde::OnSelectMultiFileDialogResponse,
                     this, parent));
}

void SelectFileDialogLinuxKde::CreateSaveAsDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::AcceleratedWidget parent) {
  pipe_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxKde::CallKDialogOutput, this,
          KDialogParams(
              "--getsavefilename", GetTitle(title, IDS_SAVE_AS_DIALOG_TITLE),
              default_path.empty() ? *last_saved_path() : default_path, parent,
              true, false)),
      base::BindOnce(
          &SelectFileDialogLinuxKde::OnSelectSingleFileDialogResponse, this,
          parent));
}

void SelectFileDialogLinuxKde::SelectSingleFileHelper(
    bool allow_folder,
    std::unique_ptr<KDialogOutputParams> results) {
  VLOG(1) << "[kdialog] SingleFileResponse: " << results->output;
  if (results->exit_code || results->output.empty()) {
    FileNotSelected();
    return;
  }

  base::FilePath path(results->output);
  if (allow_folder) {
    FileSelected(path);
    return;
  }

  if (CallDirectoryExistsOnUIThread(path))
    FileNotSelected();
  else
    FileSelected(path);
}

void SelectFileDialogLinuxKde::OnSelectSingleFileDialogResponse(
    gfx::AcceleratedWidget parent,
    std::unique_ptr<KDialogOutputParams> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parents_.erase(parent);
  SelectSingleFileHelper(false, std::move(results));
}

void SelectFileDialogLinuxKde::OnSelectSingleFolderDialogResponse(
    gfx::AcceleratedWidget parent,
    std::unique_ptr<KDialogOutputParams> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parents_.erase(parent);
  SelectSingleFileHelper(true, std::move(results));
}

void SelectFileDialogLinuxKde::OnSelectMultiFileDialogResponse(
    gfx::AcceleratedWidget parent,
    std::unique_ptr<KDialogOutputParams> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "[kdialog] MultiFileResponse: " << results->output;

  parents_.erase(parent);

  if (results->exit_code || results->output.empty()) {
    FileNotSelected();
    return;
  }

  std::vector<base::FilePath> filenames_fp;
  for (std::string_view line :
       base::SplitStringPiece(results->output, "\n", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path(line);
    if (CallDirectoryExistsOnUIThread(path))
      continue;
    filenames_fp.push_back(path);
  }

  if (filenames_fp.empty()) {
    FileNotSelected();
    return;
  }
  MultiFilesSelected(filenames_fp);
}

}  // namespace ui

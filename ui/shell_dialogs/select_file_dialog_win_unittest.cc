// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/shell_dialogs/select_file_dialog_win.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// The default title for the various dialogs.
constexpr wchar_t kSelectFolderDefaultTitle[] = L"Select Folder";
constexpr wchar_t kSelectFileDefaultTitle[] = L"Open";
constexpr wchar_t kSaveFileDefaultTitle[] = L"Save As";

// Returns the title of |window|.
std::wstring GetWindowTitle(HWND window) {
  wchar_t buffer[256];
  UINT count = ::GetWindowText(window, buffer, std::size(buffer));
  return std::wstring(buffer, count);
}

// Waits for a dialog window whose title is |dialog_title| to show and returns
// its handle.
HWND WaitForDialogWindow(const std::wstring& dialog_title) {
  // File dialogs uses this class name.
  static constexpr wchar_t kDialogClassName[] = L"#32770";

  HWND result = nullptr;
  base::TimeDelta max_wait_time = TestTimeouts::action_timeout();
  base::TimeDelta retry_interval = base::Milliseconds(20);
  while (!result && (max_wait_time.InMilliseconds() > 0)) {
    result = ::FindWindow(kDialogClassName, dialog_title.c_str());
    base::PlatformThread::Sleep(retry_interval);
    max_wait_time -= retry_interval;
  }

  if (!result) {
    LOG(ERROR) << "Wait for dialog window timed out.";
  }

  // Check the name of the dialog specifically. That's because if multiple file
  // dialogs are opened in quick successions (e.g. from one test to another),
  // the ::FindWindow() call above will work for both the previous dialog title
  // and the current.
  return GetWindowTitle(result) == dialog_title ? result : nullptr;
}

struct EnumWindowsParam {
  // The owner of the dialog. This is used to differentiate the dialog prompt
  // from the file dialog since they could have the same title.
  HWND owner;

  // Holds the resulting window.
  HWND result;
};

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM param) {
  EnumWindowsParam* enum_param = reinterpret_cast<EnumWindowsParam*>(param);

  // Early continue if the current hwnd is the file dialog.
  if (hwnd == enum_param->owner)
    return TRUE;

  // Only consider visible windows.
  if (!::IsWindowVisible(hwnd))
    return TRUE;

  // If the window doesn't have |enum_param->owner| as the owner, it can't be
  // the prompt dialog.
  if (::GetWindow(hwnd, GW_OWNER) != enum_param->owner)
    return TRUE;

  enum_param->result = hwnd;
  return FALSE;
}

HWND WaitForDialogPrompt(HWND owner) {
  // The dialog prompt could have the same title as the file dialog. This means
  // that it would not be possible to make sure the right window is found using
  // ::FindWindow(). Instead enumerate all top-level windows and return the one
  // whose owner is the file dialog.
  EnumWindowsParam param = {owner, nullptr};
  base::TimeDelta max_wait_time = TestTimeouts::action_timeout();
  base::TimeDelta retry_interval = base::Milliseconds(20);
  while (!param.result && (max_wait_time.InMilliseconds() > 0)) {
    ::EnumWindows(&EnumWindowsCallback, reinterpret_cast<LPARAM>(&param));
    base::PlatformThread::Sleep(retry_interval);
    max_wait_time -= retry_interval;
  }
  if (!param.result) {
    LOG(ERROR) << "Wait for dialog prompt timed out.";
  }
  return param.result;
}

// Returns the text of the dialog item in |window| whose id is |dialog_item_id|.
std::wstring GetDialogItemText(HWND window, int dialog_item_id) {
  if (!window)
    return std::wstring();

  wchar_t buffer[256];
  UINT count =
      ::GetDlgItemText(window, dialog_item_id, buffer, std::size(buffer));
  return std::wstring(buffer, count);
}

// Sends a command to |window| using PostMessage().
void SendCommand(HWND window, int id) {
  ASSERT_TRUE(window);

  // Make sure the window is visible first or the WM_COMMAND may not have any
  // effect.
  base::TimeDelta max_wait_time = TestTimeouts::action_timeout();
  base::TimeDelta retry_interval = base::Milliseconds(20);
  while (!::IsWindowVisible(window) && (max_wait_time.InMilliseconds() > 0)) {
    base::PlatformThread::Sleep(retry_interval);
    max_wait_time -= retry_interval;
  }
  if (!::IsWindowVisible(window)) {
    LOG(ERROR) << "SendCommand timed out.";
  }
  ::PostMessage(window, WM_COMMAND, id, 0);
}

}  // namespace

class SelectFileDialogWinTest : public ::testing::Test,
                                public ui::SelectFileDialog::Listener {
 public:
  SelectFileDialogWinTest() = default;

  SelectFileDialogWinTest(const SelectFileDialogWinTest&) = delete;
  SelectFileDialogWinTest& operator=(const SelectFileDialogWinTest&) = delete;

  ~SelectFileDialogWinTest() override = default;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    selected_paths_.push_back(file.path());
  }
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {
    selected_paths_ = ui::SelectedFileInfoListToFilePathList(files);
  }
  void FileSelectionCanceled() override { was_cancelled_ = true; }

  // Runs the scheduler until no tasks are executing anymore.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  const std::vector<base::FilePath>& selected_paths() {
    return selected_paths_;
  }

  // Return a fake NativeWindow. This will result in the dialog having no
  // parent window but the tests will still work.
  static gfx::NativeWindow native_window() {
    return reinterpret_cast<gfx::NativeWindow>(0);
  }

  bool was_cancelled() { return was_cancelled_; }

  // Resets the results so that this instance can be reused as a
  // SelectFileDialog listener.
  void ResetResults() {
    was_cancelled_ = false;
    selected_paths_.clear();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::vector<base::FilePath> selected_paths_;
  bool was_cancelled_ = false;
};

TEST_F(SelectFileDialogWinTest, CancelAllDialogs) {
  // Intentionally not testing SELECT_UPLOAD_FOLDER because the dialog is
  // customized for that case.
  struct {
    ui::SelectFileDialog::Type dialog_type;
    const wchar_t* dialog_title;
  } kTestCases[] = {
      {
          ui::SelectFileDialog::SELECT_FOLDER, kSelectFolderDefaultTitle,
      },
      {
          ui::SelectFileDialog::SELECT_EXISTING_FOLDER,
          kSelectFolderDefaultTitle,
      },
      {
          ui::SelectFileDialog::SELECT_SAVEAS_FILE, kSaveFileDefaultTitle,
      },
      {
          ui::SelectFileDialog::SELECT_OPEN_FILE, kSelectFileDefaultTitle,
      },
      {
          ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE, kSelectFileDefaultTitle,
      }};

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%zu", i));

    const auto& test_case = kTestCases[i];

    scoped_refptr<ui::SelectFileDialog> dialog =
        ui::SelectFileDialog::Create(this, nullptr);

    std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type_info;
    int file_type_info_index = 0;

    // The Save As dialog requires a filetype info.
    if (test_case.dialog_type == ui::SelectFileDialog::SELECT_SAVEAS_FILE) {
      file_type_info = std::make_unique<ui::SelectFileDialog::FileTypeInfo>();
      file_type_info->extensions.push_back({L"html"});
      file_type_info_index = 1;
    }

    dialog->SelectFile(test_case.dialog_type, std::u16string(),
                       base::FilePath(), file_type_info.get(),
                       file_type_info_index, std::wstring(), native_window());

    // Accept the default value.
    HWND window = WaitForDialogWindow(test_case.dialog_title);
    SendCommand(window, IDCANCEL);

    RunUntilIdle();

    EXPECT_TRUE(was_cancelled());
    EXPECT_TRUE(selected_paths().empty());

    ResetResults();
  }
}

// When using SELECT_UPLOAD_FOLDER, the title and the ok button strings are
// modified to put emphasis on the fact that the whole folder will be uploaded.
TEST_F(SelectFileDialogWinTest, UploadFolderCheckStrings) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath default_path = scoped_temp_dir.GetPath();

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_UPLOAD_FOLDER,
                     std::u16string(), default_path, nullptr, 0, L"",
                     native_window());

  // Wait for the window to open and make sure the window title was changed from
  // the default title for a regular select folder operation.
  HWND window = WaitForDialogWindow(base::UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE)));
  EXPECT_NE(GetWindowTitle(window), kSelectFolderDefaultTitle);

  // Check the OK button text.
  EXPECT_EQ(GetDialogItemText(window, 1),
            base::UTF16ToWide(l10n_util::GetStringUTF16(
                IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON)));

  // Close the dialog.
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  ASSERT_EQ(1u, selected_paths().size());
  // On some machines GetSystemDirectory returns C:\WINDOWS which is then
  // normalized to C:\Windows by the file dialog, leading to spurious failures
  // if a case-sensitive comparison is used.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      selected_paths()[0].value(), default_path.value()));
}

// Specifying the title when opening a dialog to select a file, select multiple
// files or save a file doesn't do anything.
TEST_F(SelectFileDialogWinTest, SpecifyTitle) {
  static constexpr char16_t kTitle[] = u"FooBar Title";

  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Create an existing file since it is required.
  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo.txt");
  std::string contents = "Hello test!";
  ASSERT_TRUE(base::WriteFile(default_path, contents));

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, kTitle,
                     default_path, nullptr, 0, L"", native_window());

  // Wait for the window to open. The title should be `kTitle`. Note that if
  // this hangs, it possibly is because the title changed.
  HWND window = WaitForDialogWindow(base::UTF16ToWide(kTitle));

  // Close the dialog and the result doesn't matter.
  SendCommand(window, IDCANCEL);
}

// Tests the selection of one file in both the single and multiple case. It's
// too much trouble to select a different file in the dialog so the default_path
// is used to pre-select a file and the OK button is clicked as soon as the
// dialog opens. This tests the default_path parameter and the single file
// selection.
TEST_F(SelectFileDialogWinTest, TestSelectFile) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Create an existing file since it is required.
  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo.txt");
  std::string contents = "Hello test!";
  ASSERT_TRUE(base::WriteFile(default_path, contents));

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     default_path, nullptr, 0, L"", native_window());

  // Wait for the window to open
  HWND window = WaitForDialogWindow(kSelectFileDefaultTitle);
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  ASSERT_EQ(1u, selected_paths().size());
  // On some machines GetSystemDirectory returns C:\WINDOWS which is then
  // normalized to C:\Windows by the file dialog, leading to spurious failures
  // if a case-sensitive comparison is used.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      selected_paths()[0].value(), default_path.value()));
}

// Tests that the file extension is automatically added.
TEST_F(SelectFileDialogWinTest, TestSaveFile) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo");

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({L"html"});

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
                     default_path, &file_type_info, 1, L"", native_window());

  // Wait for the window to open
  HWND window = WaitForDialogWindow(kSaveFileDefaultTitle);
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  ASSERT_EQ(1u, selected_paths().size());
  // On some machines GetSystemDirectory returns C:\WINDOWS which is then
  // normalized to C:\Windows by the file dialog, leading to spurious failures
  // if a case-sensitive comparison is used.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      selected_paths()[0].value(), default_path.AddExtension(L"html").value()));
}

// Tests that only specifying a basename as the default path works.
TEST_F(SelectFileDialogWinTest, OnlyBasename) {
  base::FilePath default_path(L"foobar.html");

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({L"html"});

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
                     default_path, &file_type_info, 1, L"", native_window());

  // Wait for the window to open
  HWND window = WaitForDialogWindow(kSaveFileDefaultTitle);
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  ASSERT_EQ(1u, selected_paths().size());
  EXPECT_EQ(selected_paths()[0].BaseName(), default_path);
}

TEST_F(SelectFileDialogWinTest, SaveAsDifferentExtension) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo.txt");

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({L"exe"});

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
                     default_path, &file_type_info, 1, L"html",
                     native_window());

  HWND window = WaitForDialogWindow(kSaveFileDefaultTitle);
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  // On some machines GetSystemDirectory returns C:\WINDOWS which is then
  // normalized to C:\Windows by the file dialog, leading to spurious failures
  // if a case-sensitive comparison is used.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      selected_paths()[0].value(), default_path.value()));
}

TEST_F(SelectFileDialogWinTest, OpenFileDifferentExtension) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo.txt");
  std::string contents = "Hello test!";
  ASSERT_TRUE(base::WriteFile(default_path, contents));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({L"exe"});

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     default_path, &file_type_info, 1, L"html",
                     native_window());

  HWND window = WaitForDialogWindow(kSelectFileDefaultTitle);
  SendCommand(window, IDOK);

  RunUntilIdle();

  EXPECT_FALSE(was_cancelled());
  // On some machines GetSystemDirectory returns C:\WINDOWS which is then
  // normalized to C:\Windows by the file dialog, leading to spurious failures
  // if a case-sensitive comparison is used.
  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(
      selected_paths()[0].value(), default_path.value()));
}

TEST_F(SelectFileDialogWinTest, SelectNonExistingFile) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath default_path =
      scoped_temp_dir.GetPath().Append(L"does-not-exist.html");

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     default_path, nullptr, 0, L"", native_window());

  HWND window = WaitForDialogWindow(kSelectFileDefaultTitle);
  SendCommand(window, IDOK);

  // Since selecting a non-existing file is not supported, a error dialog box
  // should have appeared.
  HWND error_box = WaitForDialogPrompt(window);
  SendCommand(error_box, IDOK);

  // Now actually cancel the file dialog box.
  SendCommand(window, IDCANCEL);

  RunUntilIdle();

  EXPECT_TRUE(was_cancelled());
  EXPECT_TRUE(selected_paths().empty());
}

// Tests that selecting an existing file when saving should prompt the user with
// a dialog to confirm the overwrite.
TEST_F(SelectFileDialogWinTest, SaveFileOverwritePrompt) {
  // Create some file in a test folder.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath default_path = scoped_temp_dir.GetPath().Append(L"foo.txt");
  std::string contents = "Hello test!";
  ASSERT_TRUE(base::WriteFile(default_path, contents));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({L"txt"});

  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(this, nullptr);
  dialog->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
                     default_path, &file_type_info, 1, L"", native_window());

  HWND window = WaitForDialogWindow(kSaveFileDefaultTitle);
  SendCommand(window, IDOK);

  // Check that the prompt appears and close it. By default, the "no" option is
  // selected so sending IDOK cancels the operation.
  HWND error_box = WaitForDialogPrompt(window);
  SendCommand(error_box, IDOK);

  // Cancel the dialog.
  SendCommand(window, IDCANCEL);

  RunUntilIdle();

  EXPECT_TRUE(was_cancelled());
  EXPECT_TRUE(selected_paths().empty());
}

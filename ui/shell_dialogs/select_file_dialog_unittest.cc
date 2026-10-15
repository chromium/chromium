// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

TEST(ShellDialogs, ShortenFileNameIfNeeded) {
  struct ShortenFileNameTestCase {
    base::FilePath::StringType input;
    base::FilePath::StringType expected;
  };
  auto test_cases = std::to_array<ShortenFileNameTestCase>({
      // Paths with short paths/file names don't get shortened.
      {FILE_PATH_LITERAL("folder1111/folder2222/file1.html"),
       FILE_PATH_LITERAL("folder1111/folder2222/file1.html")},

      // Path with long filename gets shortened to 255 chars
      {FILE_PATH_LITERAL("folder1111/"
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234ab"
                         "cdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxy"
                         "z1234.html"),
       FILE_PATH_LITERAL("folder1111/"
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234ab"
                         "cdefghijklmnopqrstuvwxyz1234abcdefghij.html")},

      // Long path but short filename is not truncated, handled by system open
      // file dialog.
      {FILE_PATH_LITERAL(
           "folder1111/folder2222/folder3333/folder4444/folder5555/folder6666/"
           "folder7777/folder8888/folder9999/folder0000/folder1111/folder2222/"
           "folder3333/folder4444/folder5555/folder6666/folder7777/folder8888/"
           "folder9999/folder0000/folder1111/folder2222/folder3333/folder4444/"
           "folder5555/folder6666/folder7777/folder8888/folder9999/folder0000/"
           "file1.pdf"),
       FILE_PATH_LITERAL(
           "folder1111/folder2222/folder3333/folder4444/folder5555/folder6666/"
           "folder7777/folder8888/folder9999/folder0000/folder1111/folder2222/"
           "folder3333/folder4444/folder5555/folder6666/folder7777/folder8888/"
           "folder9999/folder0000/folder1111/folder2222/folder3333/folder4444/"
           "folder5555/folder6666/folder7777/folder8888/folder9999/folder0000/"
           "file1.pdf")},

      // Long extension with total file name length < 255 is not truncated.
      {FILE_PATH_LITERAL("folder1111/folder2222/"
                         "file1."
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234"),
       FILE_PATH_LITERAL("folder1111/folder2222/"
                         "file1."
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz123"
                         "4")},

      // Long extension, medium length file name is truncated so that total
      // file name length = 255
      {FILE_PATH_LITERAL("folder1111/folder2222/"
                         "file1234567890123456789012345678901234567890123456789"
                         "0."
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234"),
       FILE_PATH_LITERAL("folder1111/folder2222/"
                         "file1234567890123456789012345678901234567890123456789"
                         "0."
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrst")},

      // Long extension and long file name -> extension truncated to 13 chars
      // and file name truncated to 255-13.
      {FILE_PATH_LITERAL("folder1111/folder2222/"
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234ab"
                         "cdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxy"
                         "z1234."
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234"),
       FILE_PATH_LITERAL("folder1111/folder2222/"
                         "abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvw"
                         "xyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghijklmnop"
                         "qrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234abcdefghi"
                         "jklmnopqrstuvwxyz1234abcdefghijklmnopqrstuvwxyz1234ab"
                         "cdefghijklmnopqrstuvwxyz1234ab.abcdefghijkl")},
  });

  for (auto& test_case : test_cases) {
    base::FilePath input =
        base::FilePath(test_case.input).NormalizePathSeparators();
    base::FilePath output =
        base::FilePath(test_case.expected).NormalizePathSeparators();
    EXPECT_EQ(output.value(),
              ui::SelectFileDialog::GetShortenedFilePath(input).value());
    EXPECT_LE(ui::SelectFileDialog::GetShortenedFilePath(input)
                  .BaseName()
                  .value()
                  .length(),
              255u);
  }
}

namespace {

class TrackingSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  TrackingSelectFileDialogFactory(int id, int* last_called_id)
      : id_(id), last_called_id_(last_called_id) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    if (last_called_id_) {
      *last_called_id_ = id_;
    }
    return nullptr;
  }

 private:
  int id_;
  raw_ptr<int> last_called_id_;
};

}  // namespace

TEST(ShellDialogs, ScopedSetSelectFileDialogFactory) {
  int last_called_id = 0;

  // Set initial factory.
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TrackingSelectFileDialogFactory>(100, &last_called_id));

  {
    ui::ScopedSetSelectFileDialogFactory scoped_factory1(
        std::make_unique<TrackingSelectFileDialogFactory>(1, &last_called_id));
    ui::SelectFileDialog::Create(nullptr, nullptr);
    EXPECT_EQ(last_called_id, 1);

    {
      ui::ScopedSetSelectFileDialogFactory scoped_factory2(
          std::make_unique<TrackingSelectFileDialogFactory>(
              2, &last_called_id));
      ui::SelectFileDialog::Create(nullptr, nullptr);
      EXPECT_EQ(last_called_id, 2);
    }

    // After scoped_factory2 is destroyed, factory1 should be restored.
    ui::SelectFileDialog::Create(nullptr, nullptr);
    EXPECT_EQ(last_called_id, 1);
  }

  // After scoped_factory1 is destroyed, the initial factory should be restored.
  ui::SelectFileDialog::Create(nullptr, nullptr);
  EXPECT_EQ(last_called_id, 100);

  // Clean up.
  ui::SelectFileDialog::SetFactory(nullptr);
}

#if BUILDFLAG(IS_ANDROID)
namespace {

class TestSelectFileDialogListener : public ui::SelectFileDialog::Listener {
 public:
  void FileSelectionCanceled() override { canceled_ = true; }
  bool canceled() const { return canceled_; }

 private:
  bool canceled_ = false;
};

}  // namespace

TEST(ShellDialogs, SelectFileWithNullOwningWindowCancels) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestSelectFileDialogListener listener;
  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(&listener, nullptr);
  ASSERT_TRUE(dialog);

  dialog->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
                     base::FilePath(FILE_PATH_LITERAL("foo.txt")), nullptr, 0,
                     base::FilePath::StringType(), gfx::NativeWindow(),
                     nullptr);
  EXPECT_FALSE(listener.canceled());
  task_environment.RunUntilIdle();
  EXPECT_TRUE(listener.canceled());
  dialog->ListenerDestroyed();
}
#endif  // BUILDFLAG(IS_ANDROID)

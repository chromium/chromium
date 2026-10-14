// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/select_file_dialog_linux_gtk.h"

#include <gtk/gtk.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui_platform_stub.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace gtk {

namespace {

class TestListener : public ui::SelectFileDialog::Listener {
 public:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    selected_path_ = file.path();
  }

  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {
    for (const auto& file : files) {
      selected_paths_.push_back(file.path());
    }
  }

  void FileSelectionCanceled() override { canceled_ = true; }

  bool canceled_ = false;
  base::FilePath selected_path_;
  std::vector<base::FilePath> selected_paths_;
};

}  // namespace

class SelectFileDialogLinuxGtkTest : public testing::Test {
 public:
  SelectFileDialogLinuxGtkTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SelectFileDialogLinuxGtkTest() override = default;

 protected:
  void SetUp() override {
    SelectFileDialogLinuxGtk::ResetForTesting();
    platform_ = std::make_unique<GtkUiPlatformStub>();
    listener_ = std::make_unique<TestListener>();
    dialog_ = base::MakeRefCounted<SelectFileDialogLinuxGtk>(
        listener_.get(), nullptr, platform_.get());
  }

  void TearDown() override {
    dialog_.reset();
    listener_.reset();
    platform_.reset();
    SelectFileDialogLinuxGtk::ResetForTesting();
  }

  GtkWidget* GetDialogWidget() {
    if (dialog_->dialogs_.empty()) {
      return nullptr;
    }
    return dialog_->dialogs_.begin()->first;
  }

  bool IsRunning(gfx::NativeWindow parent_window) const {
    return dialog_->IsRunning(parent_window);
  }

  void OnSelectSingleFileDialogResponse(GtkWidget* dialog, int response_id) {
    dialog_->OnSelectSingleFileDialogResponse(dialog, response_id);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<GtkUiPlatformStub> platform_;
  std::unique_ptr<TestListener> listener_;
  scoped_refptr<SelectFileDialogLinuxGtk> dialog_;
};

// Tests that dialog creation is deferred when an activation key (e.g. Enter) is
// held down, and only opens after the key is released.
TEST_F(SelectFileDialogLinuxGtkTest, DeferDialogUntilActivationKeyReleased) {
  SelectFileDialogLinuxGtk::SetActivationKeyDownForTesting(true);

  dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, u"Open File",
                      base::FilePath(), nullptr, 0,
                      base::FilePath::StringType(), nullptr);

  // Dialog must not be created yet while the activation key is held down.
  EXPECT_FALSE(GetDialogWidget());
  EXPECT_TRUE(IsRunning(nullptr));

  // Release the key.
  SelectFileDialogLinuxGtk::SetActivationKeyDownForTesting(false);

  // Now the dialog should be created and visible.
  EXPECT_TRUE(GetDialogWidget());
  EXPECT_TRUE(IsRunning(nullptr));
}

// Tests that an accept response is ignored during the 500ms cooldown period
// and accepted after the cooldown period elapses.
TEST_F(SelectFileDialogLinuxGtkTest, AcceptResponseCooldown) {
  dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, u"Open File",
                      base::FilePath(), nullptr, 0,
                      base::FilePath::StringType(), nullptr);

  GtkWidget* widget = GetDialogWidget();
  ASSERT_TRUE(widget);

  // Attempt to accept at t=100ms (< 500ms).
  task_environment_.FastForwardBy(base::Milliseconds(100));
  OnSelectSingleFileDialogResponse(widget, GTK_RESPONSE_ACCEPT);

  // File must NOT have been selected, dialog must still be open.
  EXPECT_TRUE(listener_->selected_path_.empty());
  EXPECT_FALSE(listener_->canceled_);
  EXPECT_EQ(GetDialogWidget(), widget);

  // Advance time past the 500ms cooldown (e.g. another 450ms -> 550ms total).
  task_environment_.FastForwardBy(base::Milliseconds(450));

  // Simulate user response after cooldown.
  OnSelectSingleFileDialogResponse(widget, GTK_RESPONSE_ACCEPT);

  // The accept response is now processed (even if no file path was selected in
  // stub dialog, FileNotSelected is called and dialog is closed).
  EXPECT_FALSE(GetDialogWidget());
}

// Tests that cancel responses are not delayed by the cooldown.
TEST_F(SelectFileDialogLinuxGtkTest, CancelResponseImmediatelyAllowed) {
  dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, u"Open File",
                      base::FilePath(), nullptr, 0,
                      base::FilePath::StringType(), nullptr);

  GtkWidget* widget = GetDialogWidget();
  ASSERT_TRUE(widget);

  // Cancel immediately at t=0ms.
  OnSelectSingleFileDialogResponse(widget, GTK_RESPONSE_CANCEL);

  // Cancel must be processed immediately without waiting for cooldown.
  EXPECT_TRUE(listener_->canceled_);
  EXPECT_FALSE(GetDialogWidget());
}

// Tests that pressing and releasing the Enter key via ui::KeyEvent properly
// defers and then opens the dialog.
TEST_F(SelectFileDialogLinuxGtkTest,
       DeferDialogUntilEnterKeyReleasedViaKeyEvent) {
  ui::KeyEvent enter_press(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
  SelectFileDialogLinuxGtk::DispatchKeyEventForTesting(&enter_press);

  dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, u"Open File",
                      base::FilePath(), nullptr, 0,
                      base::FilePath::StringType(), nullptr);

  EXPECT_FALSE(GetDialogWidget());
  EXPECT_TRUE(IsRunning(nullptr));

  // Auto-repeat press events should keep the dialog deferred.
  SelectFileDialogLinuxGtk::DispatchKeyEventForTesting(&enter_press);
  EXPECT_FALSE(GetDialogWidget());

  // Key release should trigger dialog mapping.
  ui::KeyEvent enter_release(ui::EventType::kKeyReleased, ui::VKEY_RETURN,
                             ui::EF_NONE);
  SelectFileDialogLinuxGtk::DispatchKeyEventForTesting(&enter_release);

  EXPECT_TRUE(GetDialogWidget());
  EXPECT_TRUE(IsRunning(nullptr));
}

// Tests that CancelMode (e.g. window blur or focus loss) clears activation keys
// and resumes any deferred dialog.
TEST_F(SelectFileDialogLinuxGtkTest, CancelModeClearsActivationKeys) {
  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  SelectFileDialogLinuxGtk::DispatchKeyEventForTesting(&space_press);

  dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, u"Open File",
                      base::FilePath(), nullptr, 0,
                      base::FilePath::StringType(), nullptr);

  EXPECT_FALSE(GetDialogWidget());

  SelectFileDialogLinuxGtk::DispatchCancelModeForTesting();

  EXPECT_TRUE(GetDialogWidget());
  EXPECT_TRUE(IsRunning(nullptr));
}

}  // namespace gtk

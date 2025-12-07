// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

namespace {

// Mock listener for testing. Methods are not called in these tests.
class MockSelectFileDialogListener : public SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener() = default;
  ~MockSelectFileDialogListener() override = default;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {}
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {}
  void FileSelectionCanceled() override {}
};

}  // namespace

class SelectFileDialogLinuxPortalTest : public testing::Test {
 public:
  SelectFileDialogLinuxPortalTest() = default;
  ~SelectFileDialogLinuxPortalTest() override = default;

  void SetUp() override {
    listener_ = std::make_unique<MockSelectFileDialogListener>();
  }

  // Simulates posting a callback from a background thread (e.g., DBus thread)
  // back to the UI thread with a weak pointer, then incrementing count if
  // valid.
  void SimulateBackgroundThreadCallback(
      base::Thread& thread,
      scoped_refptr<base::SequencedTaskRunner> ui_runner,
      base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr,
      int* count) {
    thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PostCallbackToUIThread, ui_runner, weak_ptr, count));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
  std::unique_ptr<MockSelectFileDialogListener> listener_;

 private:
  static void PostCallbackToUIThread(
      scoped_refptr<base::SequencedTaskRunner> ui_runner,
      base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr,
      int* count) {
    ui_runner->PostTask(FROM_HERE,
                        base::BindOnce(&IncrementIfValid, weak_ptr, count));
  }

  static void IncrementIfValid(base::WeakPtr<SelectFileDialogLinuxPortal> ptr,
                               int* count) {
    if (ptr) {
      (*count)++;
    }
  }
};

class TestableSelectFileDialogLinuxPortal : public SelectFileDialogLinuxPortal {
 public:
  explicit TestableSelectFileDialogLinuxPortal(Listener* listener)
      : SelectFileDialogLinuxPortal(listener,
                                    std::unique_ptr<ui::SelectFilePolicy>()) {}

  using SelectFileDialogLinuxPortal::listener_;
  using SelectFileDialogLinuxPortal::ListenerDestroyed;

  base::WeakPtr<SelectFileDialogLinuxPortal> GetWeakPtrForTesting() {
    return SelectFileDialogLinuxPortal::GetWeakPtrForTesting();
  }

 protected:
  ~TestableSelectFileDialogLinuxPortal() override = default;
};

// Tests that weak pointers work correctly before ListenerDestroyed is called.
TEST_F(SelectFileDialogLinuxPortalTest, WeakPtrsWorkBeforeListenerDestroyed) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  bool callback_invoked = false;
  base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr =
      dialog->GetWeakPtrForTesting();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SelectFileDialogLinuxPortal> ptr, bool* invoked) {
            if (ptr) {
              *invoked = true;
            }
          },
          weak_ptr, &callback_invoked));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_invoked);
}

// Tests that ListenerDestroyed invalidates weak pointers and calls parent.
TEST_F(SelectFileDialogLinuxPortalTest,
       ListenerDestroyedInvalidatesAndCallsParent) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr =
      dialog->GetWeakPtrForTesting();
  EXPECT_TRUE(weak_ptr);
  EXPECT_TRUE(dialog->listener_);

  dialog->ListenerDestroyed();

  EXPECT_FALSE(weak_ptr);
  EXPECT_FALSE(dialog->listener_);
}

// Tests that weak pointer invalidation prevents callbacks from multiple racing
// threads.
TEST_F(SelectFileDialogLinuxPortalTest,
       MultipleThreadsRacingWithListenerDestroyed) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  base::Thread dbus_thread_1("TestDBusThread1");
  base::Thread dbus_thread_2("TestDBusThread2");
  ASSERT_TRUE(dbus_thread_1.Start());
  ASSERT_TRUE(dbus_thread_2.Start());

  int callbacks_invoked = 0;
  base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr =
      dialog->GetWeakPtrForTesting();
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  for (int i = 0; i < 3; ++i) {
    SimulateBackgroundThreadCallback(dbus_thread_1, ui_task_runner, weak_ptr,
                                     &callbacks_invoked);
    SimulateBackgroundThreadCallback(dbus_thread_2, ui_task_runner, weak_ptr,
                                     &callbacks_invoked);
  }

  dbus_thread_1.FlushForTesting();
  dbus_thread_2.FlushForTesting();

  dialog->ListenerDestroyed();

  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, callbacks_invoked);

  dbus_thread_1.Stop();
  dbus_thread_2.Stop();
}

}  // namespace ui

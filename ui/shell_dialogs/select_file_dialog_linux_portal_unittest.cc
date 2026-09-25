// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/display/test/test_screen.h"
#include "ui/ozone/public/ozone_switches.h"
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

class FakeWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  explicit FakeWindowTreeHost(base::OnceClosure on_release_capture)
      : on_release_capture_(std::move(on_release_capture)) {
    window()->Init(ui::LAYER_NOT_DRAWN);
  }
  ~FakeWindowTreeHost() override = default;

  void ReleaseCapture() override {
    if (on_release_capture_) {
      std::move(on_release_capture_).Run();
    }
  }

 private:
  base::OnceClosure on_release_capture_;
};

}  // namespace

class SelectFileDialogLinuxPortalTest : public testing::Test {
 public:
  SelectFileDialogLinuxPortalTest() = default;
  ~SelectFileDialogLinuxPortalTest() override = default;

  void SetUp() override {
    // The test may run without a display server, so use the headless Ozone
    // platform.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOzonePlatform, "headless");
    env_ = aura::Env::CreateInstance();
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
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<aura::Env> env_;
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
                                    std::unique_ptr<ui::SelectFilePolicy>()) {
    invoker_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  using SelectFileDialogLinuxPortal::DialogCreatedOnInvoker;
  using SelectFileDialogLinuxPortal::host_;
  using SelectFileDialogLinuxPortal::listener_;
  using SelectFileDialogLinuxPortal::ListenerDestroyed;
  using SelectFileDialogLinuxPortal::reenable_window_event_handling_;

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

  bool reenabled_events = false;
  dialog->reenable_window_event_handling_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &reenabled_events);

  base::WeakPtr<SelectFileDialogLinuxPortal> weak_ptr =
      dialog->GetWeakPtrForTesting();
  EXPECT_TRUE(weak_ptr);
  EXPECT_TRUE(dialog->listener_);

  dialog->ListenerDestroyed();

  EXPECT_FALSE(weak_ptr);
  EXPECT_FALSE(dialog->listener_);
  EXPECT_TRUE(reenabled_events);
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

// Regression test for crbug.com/565349184: Releasing capture in
// DialogCreatedOnInvoker() can synchronously cancel a tab drag, deactivate the
// tab, and destroy the dialog before ReleaseCapture() returns.
TEST_F(SelectFileDialogLinuxPortalTest,
       DialogDestroyedDuringReleaseCaptureInDialogCreatedOnInvoker) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  base::WeakPtr<SelectFileDialogLinuxPortal> weak_dialog =
      dialog->GetWeakPtrForTesting();

  FakeWindowTreeHost host(base::BindOnce(
      [](scoped_refptr<TestableSelectFileDialogLinuxPortal>* dialog_ref) {
        (*dialog_ref)->ListenerDestroyed();
        dialog_ref->reset();
      },
      &dialog));
  dialog->host_ = host.GetWeakPtr();

  TestableSelectFileDialogLinuxPortal* raw_dialog = dialog.get();
  raw_dialog->DialogCreatedOnInvoker();

  EXPECT_FALSE(dialog);
  EXPECT_FALSE(weak_dialog);
}

// Tests that if the WindowTreeHost is destroyed synchronously during
// ReleaseCapture(), DialogCreatedOnInvoker() safely returns without
// dereferencing a null host pointer.
TEST_F(SelectFileDialogLinuxPortalTest,
       HostDestroyedDuringReleaseCaptureInDialogCreatedOnInvoker) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  std::unique_ptr<FakeWindowTreeHost> host;
  host = std::make_unique<FakeWindowTreeHost>(base::BindOnce(
      [](std::unique_ptr<FakeWindowTreeHost>* host_ptr) { host_ptr->reset(); },
      &host));
  dialog->host_ = host->GetWeakPtr();

  dialog->DialogCreatedOnInvoker();

  EXPECT_FALSE(host);
  EXPECT_FALSE(dialog->host_);
}

}  // namespace ui

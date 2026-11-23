// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/display/test/test_screen.h"
#include "ui/linux/fake_linux_ui.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/shell_dialogs/activation_key_tracker_linux.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

namespace {

// Mock listener for testing.
class MockSelectFileDialogListener : public SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener() = default;
  ~MockSelectFileDialogListener() override = default;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {}
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {}
  void FileSelectionCanceled() override { cancellations_++; }

  int cancellations() const { return cancellations_; }

 private:
  int cancellations_ = 0;
};

// A dialog that records the type of the SelectFile() calls it gets.
class RecordingSelectFileDialog : public SelectFileDialog {
 public:
  RecordingSelectFileDialog(Listener* listener, std::vector<Type>* calls)
      : SelectFileDialog(listener, nullptr), calls_(calls) {}

  // SelectFileDialog:
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }

 protected:
  ~RecordingSelectFileDialog() override = default;

  // SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    calls_->push_back(type);
  }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  raw_ptr<std::vector<Type>> calls_;
};

// A LinuxUi whose file dialogs are RecordingSelectFileDialogs.
class FallbackLinuxUi : public FakeLinuxUi {
 public:
  SelectFileDialog* CreateSelectFileDialog(
      SelectFileDialog::Listener* listener,
      std::unique_ptr<SelectFilePolicy> policy) const override {
    return new RecordingSelectFileDialog(listener, &fallback_calls_);
  }

  const std::vector<SelectFileDialog::Type>& fallback_calls() const {
    return fallback_calls_;
  }

 private:
  mutable std::vector<SelectFileDialog::Type> fallback_calls_;
};

class FakeLinuxUiDelegate : public LinuxUiDelegate {
 public:
  explicit FakeLinuxUiDelegate(int x11_screen_number)
      : x11_screen_number_(x11_screen_number) {}
  ~FakeLinuxUiDelegate() override = default;

  bool exported_window_handle() const { return exported_window_handle_; }

  // LinuxUiDelegate:
  LinuxUiBackend GetBackend() const override { return LinuxUiBackend::kX11; }
  int GetX11ScreenNumber() const override { return x11_screen_number_; }
  void ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override {
    // Don't run `callback`, which would make a D-Bus portal request.
    exported_window_handle_ = true;
  }

 private:
  const int x11_screen_number_;
  bool exported_window_handle_ = false;
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

  void TearDown() override {
    ActivationKeyTracker::GetInstance()->ResetForTesting();
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
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  using SelectFileDialogLinuxPortal::IsAcceptAllowed;
  using SelectFileDialogLinuxPortal::listener_;
  using SelectFileDialogLinuxPortal::ListenerDestroyed;
  using SelectFileDialogLinuxPortal::OnFileChooserResponse;
  using SelectFileDialogLinuxPortal::OnPortalAvailable;
  using SelectFileDialogLinuxPortal::OnWidgetActivationChanged;
  using SelectFileDialogLinuxPortal::reenable_window_event_handling_;
  using SelectFileDialogLinuxPortal::StartAcceptDelay;

  base::WeakPtr<SelectFileDialogLinuxPortal> GetWeakPtrForTesting() {
    return SelectFileDialogLinuxPortal::GetWeakPtrForTesting();
  }

  // Records portal requests instead of making D-Bus calls.
  void RequestPortal(std::u16string title,
                     base::FilePath default_path,
                     base::FilePath::StringType default_extension,
                     std::optional<GURL> caller) override {
    portal_requests_++;
  }

  int portal_requests() const { return portal_requests_; }

 private:
  int portal_requests_ = 0;

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

// Tests that the portal is not requested while Enter or Space is held, so a
// held key can't accept the dialog as soon as it appears.
TEST_F(SelectFileDialogLinuxPortalTest, RequestDeferredUntilActivationKeyUp) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  ActivationKeyTracker::GetInstance()->SetActivationKeyDownForTesting(true);

  dialog->SelectFile(SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     base::FilePath(), nullptr, 0, base::FilePath::StringType(),
                     gfx::NativeWindow());
  EXPECT_EQ(0, dialog->portal_requests());

  ActivationKeyTracker::GetInstance()->SetActivationKeyDownForTesting(false);
  EXPECT_EQ(1, dialog->portal_requests());
}

// Tests that the portal is requested immediately when no activation key is
// held.
TEST_F(SelectFileDialogLinuxPortalTest, RequestImmediateWithoutActivationKey) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  ActivationKeyTracker::GetInstance()->SetActivationKeyDownForTesting(false);

  dialog->SelectFile(SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     base::FilePath(), nullptr, 0, base::FilePath::StringType(),
                     gfx::NativeWindow());
  EXPECT_EQ(1, dialog->portal_requests());
}

// Tests that a pending request is dropped if the listener goes away while an
// activation key is held.
TEST_F(SelectFileDialogLinuxPortalTest, DeferredRequestDroppedAfterDestroy) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  ActivationKeyTracker::GetInstance()->SetActivationKeyDownForTesting(true);

  dialog->SelectFile(SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     base::FilePath(), nullptr, 0, base::FilePath::StringType(),
                     gfx::NativeWindow());
  dialog->ListenerDestroyed();
  ActivationKeyTracker::GetInstance()->SetActivationKeyDownForTesting(false);
  EXPECT_EQ(0, dialog->portal_requests());
}

// Tests that accept responses are rejected until the dialog could have been
// visible for kFileDialogMinDisplayDuration.
TEST_F(SelectFileDialogLinuxPortalTest, AcceptRejectedUntilMinDisplayDuration) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  dialog->StartAcceptDelay();
  EXPECT_FALSE(dialog->IsAcceptAllowed());

  task_environment_.FastForwardBy(kFileDialogMinDisplayDuration -
                                  base::Milliseconds(1));
  EXPECT_FALSE(dialog->IsAcceptAllowed());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(dialog->IsAcceptAllowed());
}

// Tests that the parent losing activation, which happens when the portal
// dialog maps, restarts the accept delay.
TEST_F(SelectFileDialogLinuxPortalTest, DeactivationRestartsAcceptDelay) {
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());

  dialog->StartAcceptDelay();
  task_environment_.FastForwardBy(base::Milliseconds(400));
  dialog->OnWidgetActivationChanged(nullptr, /*active=*/false);

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(dialog->IsAcceptAllowed());

  // Regaining activation doesn't restart the delay.
  dialog->OnWidgetActivationChanged(nullptr, /*active=*/true);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(dialog->IsAcceptAllowed());
}

// Tests that the fallback dialog is shown when the portal rejects the file
// chooser call, e.g. with org.freedesktop.DBus.Error.AccessDenied.
TEST_F(SelectFileDialogLinuxPortalTest,
       FallbackDialogShownOnMethodCallFailure) {
  FallbackLinuxUi linux_ui;
  LinuxUi* old_linux_ui = LinuxUi::SetInstance(&linux_ui);
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  dialog->SelectFile(SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
                     base::FilePath(), nullptr, 0, base::FilePath::StringType(),
                     gfx::NativeWindow());

  dialog->OnFileChooserResponse(
      base::unexpected(dbus_xdg::ResponseError::kMethodCallFailed));

  EXPECT_EQ(
      std::vector<SelectFileDialog::Type>{SelectFileDialog::SELECT_OPEN_FILE},
      linux_ui.fallback_calls());
  EXPECT_EQ(0, listener_->cancellations());

  dialog->ListenerDestroyed();
  LinuxUi::SetInstance(old_linux_ui);
}

// Tests that the portal is used on X11 screen 0.
TEST_F(SelectFileDialogLinuxPortalTest, UsesPortalOnX11Screen0) {
  FakeLinuxUiDelegate delegate(/*x11_screen_number=*/0);
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  FakeWindowTreeHost host(base::DoNothing());
  dialog->host_ = host.GetWeakPtr();

  dialog->OnPortalAvailable(u"", base::FilePath(), "", std::nullopt,
                            /*version=*/3);

  EXPECT_TRUE(delegate.exported_window_handle());
  EXPECT_EQ(0, listener_->cancellations());
  dialog->ListenerDestroyed();
}

// Regression test for crbug.com/40917377: The portal shows its dialog on X11
// screen 0, so it must not be used when Chrome runs on another screen of a
// multi-screen ("Zaphod") X server. The in-process dialog is used instead,
// which cancels here since there is no LinuxUi in this test.
TEST_F(SelectFileDialogLinuxPortalTest, NoPortalOnOtherX11Screen) {
  FakeLinuxUiDelegate delegate(/*x11_screen_number=*/1);
  auto dialog = base::MakeRefCounted<TestableSelectFileDialogLinuxPortal>(
      listener_.get());
  FakeWindowTreeHost host(base::DoNothing());
  dialog->host_ = host.GetWeakPtr();

  dialog->OnPortalAvailable(u"", base::FilePath(), "", std::nullopt,
                            /*version=*/3);

  EXPECT_FALSE(delegate.exported_window_handle());
  EXPECT_EQ(1, listener_->cancellations());
  dialog->ListenerDestroyed();
}

}  // namespace ui

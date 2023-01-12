// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/event.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/win/windows_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_input_pane.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_tab_tip.h"

namespace ui {

class MockVirtualKeyboardControllerObserver
    : public VirtualKeyboardControllerObserver {
 public:
  MockVirtualKeyboardControllerObserver() = default;

  MockVirtualKeyboardControllerObserver(
      const MockVirtualKeyboardControllerObserver&) = delete;
  MockVirtualKeyboardControllerObserver& operator=(
      const MockVirtualKeyboardControllerObserver&) = delete;

  ~MockVirtualKeyboardControllerObserver() override = default;

  MOCK_METHOD1(OnKeyboardVisible, void(const gfx::Rect&));
  MOCK_METHOD0(OnKeyboardHidden, void());
};

class MockInputPane
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::ViewManagement::IInputPane2,
          ABI::Windows::UI::ViewManagement::IInputPane> {
 public:
  using InputPaneEventHandler = ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::UI::ViewManagement::InputPane*,
      ABI::Windows::UI::ViewManagement::InputPaneVisibilityEventArgs*>;

  MockInputPane() = default;

  MockInputPane(const MockInputPane&) = delete;
  MockInputPane& operator=(const MockInputPane&) = delete;

  IFACEMETHODIMP TryShow(boolean*) override {
    if (showing_)
      return S_OK;
    showing_ = true;
    EXPECT_NE(nullptr, show_handler_.Get());
    show_handler_->Invoke(this, nullptr);
    return S_OK;
  }

  IFACEMETHODIMP TryHide(boolean*) override {
    if (!showing_)
      return S_OK;
    showing_ = false;
    EXPECT_NE(nullptr, hide_handler_.Get());
    hide_handler_->Invoke(this, nullptr);
    return S_OK;
  }

  IFACEMETHODIMP
  add_Showing(InputPaneEventHandler* handler,
              EventRegistrationToken* token) override {
    EXPECT_EQ(nullptr, show_handler_.Get());
    show_handler_ = handler;
    return S_OK;
  }
  IFACEMETHODIMP
  remove_Showing(EventRegistrationToken token) override {
    EXPECT_NE(nullptr, show_handler_.Get());
    show_handler_.Reset();
    return S_OK;
  }
  IFACEMETHODIMP add_Hiding(InputPaneEventHandler* handler,
                            EventRegistrationToken* token) override {
    EXPECT_EQ(nullptr, hide_handler_.Get());
    hide_handler_ = handler;
    return S_OK;
  }
  IFACEMETHODIMP
  remove_Hiding(EventRegistrationToken token) override {
    EXPECT_NE(nullptr, hide_handler_.Get());
    hide_handler_.Reset();
    return S_OK;
  }
  IFACEMETHODIMP
  get_OccludedRect(ABI::Windows::Foundation::Rect* rect) override {
    rect->X = rect->Y = rect->Width = rect->Height = showing_ ? 10 : 0;
    return S_OK;
  }

  bool CallbacksValid() const { return show_handler_ && hide_handler_; }

 private:
  ~MockInputPane() override = default;

  bool showing_ = false;
  Microsoft::WRL::ComPtr<InputPaneEventHandler> show_handler_;
  Microsoft::WRL::ComPtr<InputPaneEventHandler> hide_handler_;
};

class OnScreenKeyboardTest : public ::testing::Test {
 public:
  OnScreenKeyboardTest(const OnScreenKeyboardTest&) = delete;
  OnScreenKeyboardTest& operator=(const OnScreenKeyboardTest&) = delete;

 protected:
  OnScreenKeyboardTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  std::unique_ptr<OnScreenKeyboardDisplayManagerTabTip> CreateTabTip() {
    return std::make_unique<OnScreenKeyboardDisplayManagerTabTip>(nullptr);
  }

  std::unique_ptr<OnScreenKeyboardDisplayManagerInputPane> CreateInputPane() {
    return std::make_unique<OnScreenKeyboardDisplayManagerInputPane>(nullptr);
  }

  void WaitForEventsWithTimeDelay(int64_t time_delta_ms = 10) {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(time_delta_ms));
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// This test validates the on screen keyboard path (tabtip.exe) which is read
// from the registry.
TEST_F(OnScreenKeyboardTest, OSKPath) {
  std::unique_ptr<OnScreenKeyboardDisplayManagerTabTip>
      keyboard_display_manager(CreateTabTip());
  EXPECT_NE(nullptr, keyboard_display_manager);

  std::wstring osk_path;
  EXPECT_TRUE(keyboard_display_manager->GetOSKPath(&osk_path));
  EXPECT_FALSE(osk_path.empty());
  EXPECT_TRUE(osk_path.find(L"tabtip.exe") != std::wstring::npos);

  // The path read from the registry can be quoted. To check for the existence
  // of the file we use the base::PathExists function which internally uses the
  // GetFileAttributes API which does not accept quoted strings. Our workaround
  // is to look for quotes in the first and last position in the string and
  // erase them.
  if ((osk_path.front() == L'"') && (osk_path.back() == L'"'))
    osk_path = osk_path.substr(1, osk_path.size() - 2);

  EXPECT_TRUE(base::PathExists(base::FilePath(osk_path)));
}

TEST_F(OnScreenKeyboardTest, InputPane) {
  // InputPane is supported only on RS1 and later.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  std::unique_ptr<OnScreenKeyboardDisplayManagerInputPane>
      keyboard_display_manager = CreateInputPane();

  std::unique_ptr<MockVirtualKeyboardControllerObserver> observer =
      std::make_unique<MockVirtualKeyboardControllerObserver>();

  Microsoft::WRL::ComPtr<MockInputPane> input_pane =
      Microsoft::WRL::Make<MockInputPane>();
  keyboard_display_manager->SetInputPaneForTesting(input_pane);

  EXPECT_CALL(*observer, OnKeyboardVisible(testing::_)).Times(1);
  keyboard_display_manager->AddObserver(observer.get());
  keyboard_display_manager->DisplayVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);

  testing::Mock::VerifyAndClearExpectations(observer.get());
  EXPECT_CALL(*observer, OnKeyboardHidden()).Times(1);
  keyboard_display_manager->DismissVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);
  keyboard_display_manager->RemoveObserver(observer.get());
}

TEST_F(OnScreenKeyboardTest, InputPaneDebounceTimerTest) {
  // InputPane is supported only on RS1 and later.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  std::unique_ptr<OnScreenKeyboardDisplayManagerInputPane>
      keyboard_display_manager = CreateInputPane();

  std::unique_ptr<MockVirtualKeyboardControllerObserver> observer =
      std::make_unique<MockVirtualKeyboardControllerObserver>();

  Microsoft::WRL::ComPtr<MockInputPane> input_pane =
      Microsoft::WRL::Make<MockInputPane>();
  keyboard_display_manager->SetInputPaneForTesting(input_pane);

  EXPECT_CALL(*observer, OnKeyboardVisible(testing::_)).Times(1);
  keyboard_display_manager->AddObserver(observer.get());
  keyboard_display_manager->DisplayVirtualKeyboard();
  keyboard_display_manager->DismissVirtualKeyboard();
  keyboard_display_manager->DisplayVirtualKeyboard();
  keyboard_display_manager->DismissVirtualKeyboard();
  keyboard_display_manager->DisplayVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);

  testing::Mock::VerifyAndClearExpectations(observer.get());
  EXPECT_CALL(*observer, OnKeyboardHidden()).Times(1);
  keyboard_display_manager->DismissVirtualKeyboard();
  keyboard_display_manager->DisplayVirtualKeyboard();
  keyboard_display_manager->DismissVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);
  keyboard_display_manager->RemoveObserver(observer.get());
}

TEST_F(OnScreenKeyboardTest, InputPaneDestruction) {
  // InputPane is supported only on RS1 and later.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  std::unique_ptr<OnScreenKeyboardDisplayManagerInputPane>
      keyboard_display_manager = CreateInputPane();

  std::unique_ptr<MockVirtualKeyboardControllerObserver> observer =
      std::make_unique<MockVirtualKeyboardControllerObserver>();

  Microsoft::WRL::ComPtr<MockInputPane> input_pane =
      Microsoft::WRL::Make<MockInputPane>();
  keyboard_display_manager->SetInputPaneForTesting(input_pane);

  EXPECT_CALL(*observer, OnKeyboardVisible(testing::_)).Times(1);
  keyboard_display_manager->AddObserver(observer.get());
  keyboard_display_manager->DisplayVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);
  EXPECT_TRUE(input_pane->CallbacksValid());

  testing::Mock::VerifyAndClearExpectations(observer.get());
  EXPECT_CALL(*observer, OnKeyboardHidden()).Times(1);
  keyboard_display_manager->DismissVirtualKeyboard();
  // Additional 300ms for debounce timer.
  WaitForEventsWithTimeDelay(400);
  keyboard_display_manager->RemoveObserver(observer.get());
  keyboard_display_manager = nullptr;
  WaitForEventsWithTimeDelay(400);
  EXPECT_FALSE(input_pane->CallbacksValid());
}

}  // namespace ui

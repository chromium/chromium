// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/on_screen_keyboard_display_manager_input_pane.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ime/input_method_keyboard_controller_observer.h"

namespace ui {

// VirtualKeyboardInputPane class is used to store all the COM objects and
// control their lifetime, so all the COM processing is on a background
// thread.
class OnScreenKeyboardDisplayManagerInputPane::VirtualKeyboardInputPane
    : public base::RefCountedThreadSafe<VirtualKeyboardInputPane> {
 public:
  explicit VirtualKeyboardInputPane(
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : main_task_runner_(task_runner) {}

  void InitVirtualKeyboardInputPaneInstance(
      base::WeakPtr<OnScreenKeyboardDisplayManagerInputPane>
          input_pane_weak_ptr) {
    keyboard_input_pane_weak_ptr_ = input_pane_weak_ptr;
  }

  // Set the virtual keyboard input pane for |OnScreenKeyboardTest| tests.
  void SetInputPaneForTestingInBackgroundThread(
      Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IInputPane>
          pane) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    DCHECK(!input_pane_);
    input_pane_ = pane;
    HRESULT hr = input_pane_.As(&input_pane2_);
    DCHECK(SUCCEEDED(hr));

    AddCallbacksOnInputPaneShownOrHiddenInBackgroundThread();
  }

  void RegisterForInputPaneVisibilityChangeInBackgroundThread(HWND hwnd) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    EnsureInputPanePointersInBackgroundThread(hwnd);
  }

 private:
  friend class base::RefCountedThreadSafe<VirtualKeyboardInputPane>;

  ~VirtualKeyboardInputPane() {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
  }

  void EnsureInputPanePointersInBackgroundThread(HWND hwnd) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    if (input_pane2_)
      return;
    if (!base::win::ResolveCoreWinRTDelayload() ||
        !base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
      return;
    }

    base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

    base::win::ScopedHString input_pane_guid = base::win::ScopedHString::Create(
        RuntimeClass_Windows_UI_ViewManagement_InputPane);
    Microsoft::WRL::ComPtr<IInputPaneInterop> input_pane_interop;
    HRESULT hr = base::win::RoGetActivationFactory(
        input_pane_guid.get(), IID_PPV_ARGS(&input_pane_interop));
    if (FAILED(hr))
      return;

    hr = input_pane_interop->GetForWindow(hwnd, IID_PPV_ARGS(&input_pane_));
    if (FAILED(hr))
      return;

    if (FAILED(input_pane_.As(&input_pane2_)))
      return;

    AddCallbacksOnInputPaneShownOrHiddenInBackgroundThread();
  }

  // Add callbacks to notify virtual keyboard observers when the virtual
  // keyboard is visible or hidden.
  void AddCallbacksOnInputPaneShownOrHiddenInBackgroundThread() {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    input_pane_->add_Showing(
        Microsoft::WRL::Callback<
            OnScreenKeyboardDisplayManagerInputPane::VirtualKeyboardInputPane::
                InputPaneEventHandler>(
            this, &OnScreenKeyboardDisplayManagerInputPane::
                      VirtualKeyboardInputPane::OnInputPaneShown)
            .Get(),
        &show_event_token_);

    input_pane_->add_Hiding(
        Microsoft::WRL::Callback<
            OnScreenKeyboardDisplayManagerInputPane::VirtualKeyboardInputPane::
                InputPaneEventHandler>(
            this, &OnScreenKeyboardDisplayManagerInputPane::
                      VirtualKeyboardInputPane::OnInputPaneHidden)
            .Get(),
        &hide_event_token_);
  }

  HRESULT OnInputPaneShown(
      ABI::Windows::UI::ViewManagement::IInputPane* pane,
      ABI::Windows::UI::ViewManagement::IInputPaneVisibilityEventArgs* args) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    ABI::Windows::Foundation::Rect rect;
    input_pane_->get_OccludedRect(&rect);
    gfx::Rect dip_rect(rect.X, rect.Y, rect.Width, rect.Height);

    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OnScreenKeyboardDisplayManagerInputPane::
                                      NotifyObserversOnKeyboardShown,
                                  keyboard_input_pane_weak_ptr_, dip_rect));
    return S_OK;
  }

  HRESULT OnInputPaneHidden(
      ABI::Windows::UI::ViewManagement::IInputPane* pane,
      ABI::Windows::UI::ViewManagement::IInputPaneVisibilityEventArgs* args) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OnScreenKeyboardDisplayManagerInputPane::
                                      NotifyObserversOnKeyboardHidden,
                                  keyboard_input_pane_weak_ptr_));
    return S_OK;
  }

  using InputPaneEventHandler = ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::UI::ViewManagement::InputPane*,
      ABI::Windows::UI::ViewManagement::InputPaneVisibilityEventArgs*>;

  // InputPane objects are owned by VirtualKeyboardInputPane class and their
  // functions are ran on a background thread.
  Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IInputPane>
      input_pane_;
  Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IInputPane2>
      input_pane2_;

  EventRegistrationToken show_event_token_;
  EventRegistrationToken hide_event_token_;

  // |main_task_runner_| and |keyboard_input_pane_weak_ptr_| are owned by
  // OnScreenKeyboardDisplayManagerInputPane class, and they are running on the
  // main thread, which are used to post task to the main thread from the
  // background thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::WeakPtr<OnScreenKeyboardDisplayManagerInputPane>
      keyboard_input_pane_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardInputPane);
};

OnScreenKeyboardDisplayManagerInputPane::
    OnScreenKeyboardDisplayManagerInputPane(HWND hwnd)
    : hwnd_(hwnd),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      background_task_runner_(
          base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock()})),
      virtual_keyboard_input_pane_(
          base::MakeRefCounted<OnScreenKeyboardDisplayManagerInputPane::
                                   VirtualKeyboardInputPane>(
              main_task_runner_)),
      is_keyboard_visible_(false) {
  DCHECK_GE(base::win::GetVersion(), base::win::Version::WIN10_RS1);
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // We post the initiation of |virtual_keyboard_input_pane_| to the background
  // thread first, and any other tasks posted to the background thread are
  // executed after its initiation.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OnScreenKeyboardDisplayManagerInputPane::VirtualKeyboardInputPane::
              InitVirtualKeyboardInputPaneInstance,
          base::RetainedRef(virtual_keyboard_input_pane_),
          weak_factory_.GetWeakPtr()));
}

bool OnScreenKeyboardDisplayManagerInputPane::DisplayVirtualKeyboard() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OnScreenKeyboardDisplayManagerInputPane::VirtualKeyboardInputPane::
              RegisterForInputPaneVisibilityChangeInBackgroundThread,
          base::RetainedRef(virtual_keyboard_input_pane_), hwnd_));
  return true;
}

void OnScreenKeyboardDisplayManagerInputPane::DismissVirtualKeyboard() {}

void OnScreenKeyboardDisplayManagerInputPane::AddObserver(
    InputMethodKeyboardControllerObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  observers_.AddObserver(observer);
}

void OnScreenKeyboardDisplayManagerInputPane::RemoveObserver(
    InputMethodKeyboardControllerObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  observers_.RemoveObserver(observer);
}

bool OnScreenKeyboardDisplayManagerInputPane::IsKeyboardVisible() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return is_keyboard_visible_;
}

void OnScreenKeyboardDisplayManagerInputPane::SetInputPaneForTesting(
    Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IInputPane> pane) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &OnScreenKeyboardDisplayManagerInputPane::
                         VirtualKeyboardInputPane::
                             SetInputPaneForTestingInBackgroundThread,
                     base::RetainedRef(virtual_keyboard_input_pane_), pane));
}

void OnScreenKeyboardDisplayManagerInputPane::NotifyObserversOnKeyboardShown(
    gfx::Rect dip_rect) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  is_keyboard_visible_ = true;
  for (InputMethodKeyboardControllerObserver& observer : observers_)
    observer.OnKeyboardVisible(dip_rect);
}

void OnScreenKeyboardDisplayManagerInputPane::
    NotifyObserversOnKeyboardHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  is_keyboard_visible_ = false;
  for (InputMethodKeyboardControllerObserver& observer : observers_)
    observer.OnKeyboardHidden();
}

OnScreenKeyboardDisplayManagerInputPane::
    ~OnScreenKeyboardDisplayManagerInputPane() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (virtual_keyboard_input_pane_.get()) {
    background_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(virtual_keyboard_input_pane_));
  }
}

}  // namespace ui

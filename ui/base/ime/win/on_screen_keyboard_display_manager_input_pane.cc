// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/on_screen_keyboard_display_manager_input_pane.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"

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

  VirtualKeyboardInputPane(const VirtualKeyboardInputPane&) = delete;
  VirtualKeyboardInputPane& operator=(const VirtualKeyboardInputPane&) = delete;

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

  void TryShowInBackgroundThread(HWND hwnd) {
    // TODO(crbug.com/40110609): Remove this once TSF fix for input pane policy
    // is serviced
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    if (!EnsureInputPanePointersInBackgroundThread(hwnd))
      return;
    boolean res;
    TRACE_EVENT0("vk",
                 "OnScreenKeyboardDisplayManagerInputPane::"
                 "VirtualKeyboardInputPane::TryShowInBackgroundThread");
    input_pane2_->TryShow(&res);
  }

  void TryHideInBackgroundThread(HWND hwnd) {
    // TODO(crbug.com/40110609): Remove this once TSF fix for input pane policy
    // is serviced
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    if (!EnsureInputPanePointersInBackgroundThread(hwnd))
      return;
    boolean res;
    TRACE_EVENT0("vk",
                 "OnScreenKeyboardDisplayManagerInputPane::"
                 "VirtualKeyboardInputPane::TryHideInBackgroundThread");
    input_pane2_->TryHide(&res);
  }

 private:
  friend class base::RefCountedThreadSafe<VirtualKeyboardInputPane>;

  ~VirtualKeyboardInputPane() {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    if (input_pane_) {
      // Remove the callbacks that were registered.
      input_pane_->remove_Showing(show_event_token_);
      input_pane_->remove_Hiding(hide_event_token_);
    }
  }

  bool EnsureInputPanePointersInBackgroundThread(HWND hwnd) {
    DCHECK(!main_task_runner_->BelongsToCurrentThread());
    if (input_pane2_)
      return true;

    base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

    base::win::ScopedHString input_pane_guid = base::win::ScopedHString::Create(
        RuntimeClass_Windows_UI_ViewManagement_InputPane);
    Microsoft::WRL::ComPtr<IInputPaneInterop> input_pane_interop;
    HRESULT hr = base::win::RoGetActivationFactory(
        input_pane_guid.get(), IID_PPV_ARGS(&input_pane_interop));
    if (FAILED(hr))
      return false;

    hr = input_pane_interop->GetForWindow(hwnd, IID_PPV_ARGS(&input_pane_));
    if (FAILED(hr))
      return false;

    if (FAILED(input_pane_.As(&input_pane2_))) {
      input_pane_.Reset();
      return false;
    }

    AddCallbacksOnInputPaneShownOrHiddenInBackgroundThread();
    return true;
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
    // Due to timing this could be called by the OS even when input_pane_
    // is null, so just bail out to avoid crashes.
    if (!input_pane_)
      return S_OK;

    ABI::Windows::Foundation::Rect rect;
    input_pane_->get_OccludedRect(&rect);
    gfx::Rect dip_rect(rect.X, rect.Y, rect.Width, rect.Height);
    TRACE_EVENT1("vk",
                 "OnScreenKeyboardDisplayManagerInputPane::"
                 "VirtualKeyboardInputPane::OnInputPaneShown",
                 "dip_rect", dip_rect.ToString());

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
    // Due to timing this could be called by the OS even when input_pane_
    // is null, so just bail out to avoid crashes.
    if (!input_pane_)
      return S_OK;

    TRACE_EVENT0("vk",
                 "OnScreenKeyboardDisplayManagerInputPane::"
                 "VirtualKeyboardInputPane::OnInputPaneHidden");
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
};

OnScreenKeyboardDisplayManagerInputPane::
    OnScreenKeyboardDisplayManagerInputPane(HWND hwnd)
    : hwnd_(hwnd),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      background_task_runner_(base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(),
           // This TaskRunner runs tasks that wait for messages to be processed
           // on the main thread. During shutdown, the main thread stops
           // processing messages and as a result tasks on this TaskRunner may
           // hang. Use `CONTINUE_ON_SHUTDOWN` to let shutdown complete when
           // this happens (see crbug.com/40848571).
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      virtual_keyboard_input_pane_(
          base::MakeRefCounted<OnScreenKeyboardDisplayManagerInputPane::
                                   VirtualKeyboardInputPane>(
              main_task_runner_)),
      is_keyboard_visible_(false) {
  DCHECK_GE(base::win::GetVersion(), base::win::Version::WIN10_RS1);
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // 300ms is the timer we chose after experimenting with users on windows touch
  // devices.
  debouncer_ = std::make_unique<VirtualKeyboardDebounceTimer>(300);

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

void OnScreenKeyboardDisplayManagerInputPane::Run() {
  // Execute show() or hide() on the background thread after the debounce
  // expires.
  switch (last_vk_visibility_request_) {
    case mojom::VirtualKeyboardVisibilityRequest::SHOW: {
      background_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &OnScreenKeyboardDisplayManagerInputPane::
                  VirtualKeyboardInputPane::TryShowInBackgroundThread,
              base::RetainedRef(virtual_keyboard_input_pane_), hwnd_));
      break;
    }
    case mojom::VirtualKeyboardVisibilityRequest::HIDE: {
      background_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &OnScreenKeyboardDisplayManagerInputPane::
                  VirtualKeyboardInputPane::TryHideInBackgroundThread,
              base::RetainedRef(virtual_keyboard_input_pane_), hwnd_));
      break;
    }
    case mojom::VirtualKeyboardVisibilityRequest::NONE: {
      break;
    }
  }
  // Reset the VK visibility state to none so we can keep track of subsequent
  // API calls.
  last_vk_visibility_request_ = mojom::VirtualKeyboardVisibilityRequest::NONE;
}

bool OnScreenKeyboardDisplayManagerInputPane::DisplayVirtualKeyboard() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  last_vk_visibility_request_ = mojom::VirtualKeyboardVisibilityRequest::SHOW;
  debouncer_->RequestRun(base::BindOnce(
      &OnScreenKeyboardDisplayManagerInputPane::Run, base::Unretained(this)));
  return true;
}

void OnScreenKeyboardDisplayManagerInputPane::DismissVirtualKeyboard() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  last_vk_visibility_request_ = mojom::VirtualKeyboardVisibilityRequest::HIDE;
  debouncer_->RequestRun(base::BindOnce(
      &OnScreenKeyboardDisplayManagerInputPane::Run, base::Unretained(this)));
}

void OnScreenKeyboardDisplayManagerInputPane::AddObserver(
    VirtualKeyboardControllerObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  observers_.AddObserver(observer);
}

void OnScreenKeyboardDisplayManagerInputPane::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {
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
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
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
  for (VirtualKeyboardControllerObserver& observer : observers_)
    observer.OnKeyboardVisible(dip_rect);
}

void OnScreenKeyboardDisplayManagerInputPane::
    NotifyObserversOnKeyboardHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  is_keyboard_visible_ = false;
  for (VirtualKeyboardControllerObserver& observer : observers_)
    observer.OnKeyboardHidden();
}

OnScreenKeyboardDisplayManagerInputPane::
    ~OnScreenKeyboardDisplayManagerInputPane() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // In-case there is a debouncer task running, cancel it.
  debouncer_->CancelRequest();
  if (virtual_keyboard_input_pane_.get()) {
    background_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(virtual_keyboard_input_pane_));
  }
}

}  // namespace ui

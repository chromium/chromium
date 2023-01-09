// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_INPUT_PANE_H_
#define UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_INPUT_PANE_H_

#include <inputpaneinterop.h>
#include <windows.ui.viewmanagement.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <memory>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/windows_types.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom-shared.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/win/virtual_keyboard_debounce_timer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class OnScreenKeyboardTest;

// This class provides an implementation of the OnScreenKeyboardDisplayManager
// that uses InputPane which is available on Windows >= 10.0.10240.0.
class COMPONENT_EXPORT(UI_BASE_IME_WIN)
    OnScreenKeyboardDisplayManagerInputPane final
    : public VirtualKeyboardController {
 public:
  explicit OnScreenKeyboardDisplayManagerInputPane(HWND hwnd);

  OnScreenKeyboardDisplayManagerInputPane(
      const OnScreenKeyboardDisplayManagerInputPane&) = delete;
  OnScreenKeyboardDisplayManagerInputPane& operator=(
      const OnScreenKeyboardDisplayManagerInputPane&) = delete;

  ~OnScreenKeyboardDisplayManagerInputPane() override;

  // VirtualKeyboardController:
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

  void SetInputPaneForTesting(
      Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IInputPane>
          pane);
  // Returns whether show/hide VK API is called from
  // VirtualKeyboardController or not.
  mojom::VirtualKeyboardVisibilityRequest
  GetLastVirtualKeyboardVisibilityRequest() const {
    return last_vk_visibility_request_;
  }

 private:
  class VirtualKeyboardInputPane;
  friend class OnScreenKeyboardTest;

  void NotifyObserversOnKeyboardShown(gfx::Rect rect);
  void NotifyObserversOnKeyboardHidden();
  // This executes when the debounce timer expires.
  void Run();

  // The main window which displays the on screen keyboard.
  const HWND hwnd_;
  base::ObserverList<VirtualKeyboardControllerObserver, false>::Unchecked
      observers_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  scoped_refptr<VirtualKeyboardInputPane> virtual_keyboard_input_pane_;
  bool is_keyboard_visible_;
  mojom::VirtualKeyboardVisibilityRequest last_vk_visibility_request_ =
      mojom::VirtualKeyboardVisibilityRequest::NONE;
  std::unique_ptr<VirtualKeyboardDebounceTimer> debouncer_;
  base::WeakPtrFactory<OnScreenKeyboardDisplayManagerInputPane> weak_factory_{
      this};
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_INPUT_PANE_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_
#define UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_

#include <string>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class OnScreenKeyboardDetector;

// This class provides an implementation of the VirtualKeyboardController
// that uses heuristics and the TabTip.exe to display the on screen keyboard.
// Used on Windows > 7 and Windows < 10.0.10240.0
class COMPONENT_EXPORT(UI_BASE_IME_WIN)
    OnScreenKeyboardDisplayManagerTabTip final
    : public VirtualKeyboardController {
 public:
  OnScreenKeyboardDisplayManagerTabTip(HWND hwnd);

  OnScreenKeyboardDisplayManagerTabTip(
      const OnScreenKeyboardDisplayManagerTabTip&) = delete;
  OnScreenKeyboardDisplayManagerTabTip& operator=(
      const OnScreenKeyboardDisplayManagerTabTip&) = delete;

  ~OnScreenKeyboardDisplayManagerTabTip() override;

  // VirtualKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

  // Returns the path of the on screen keyboard exe (TabTip.exe) in the
  // |osk_path| parameter.
  // Returns true on success.
  bool GetOSKPath(std::wstring* osk_path);

 private:
  friend class OnScreenKeyboardTest;
  friend class OnScreenKeyboardDetector;

  void NotifyKeyboardVisible(const gfx::Rect& occluded_rect);
  void NotifyKeyboardHidden();

  std::unique_ptr<OnScreenKeyboardDetector> keyboard_detector_;
  base::ObserverList<VirtualKeyboardControllerObserver, false>::Unchecked
      observers_;
  HWND hwnd_;

  // The location of TabTip.exe.
  std::wstring osk_path_;
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAB_TIP_H_

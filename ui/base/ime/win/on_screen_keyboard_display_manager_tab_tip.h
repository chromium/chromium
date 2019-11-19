// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAP_TIP_H_
#define UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAP_TIP_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class OnScreenKeyboardDetector;

// This class provides an implementation of the InputMethodKeyboardController
// that uses heuristics and the TabTip.exe to display the on screen keyboard.
// Used on Windows > 7 and Windows < 10.0.10240.0
class COMPONENT_EXPORT(UI_BASE_IME_WIN)
    OnScreenKeyboardDisplayManagerTabTip final
    : public InputMethodKeyboardController {
 public:
  OnScreenKeyboardDisplayManagerTabTip(HWND hwnd);
  ~OnScreenKeyboardDisplayManagerTabTip() override;

  // InputMethodKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(InputMethodKeyboardControllerObserver* observer) override;
  void RemoveObserver(InputMethodKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

  // Returns the path of the on screen keyboard exe (TabTip.exe) in the
  // |osk_path| parameter.
  // Returns true on success.
  bool GetOSKPath(base::string16* osk_path);

 private:
  friend class OnScreenKeyboardTest;
  friend class OnScreenKeyboardDetector;

  void NotifyKeyboardVisible(const gfx::Rect& occluded_rect);
  void NotifyKeyboardHidden();

  std::unique_ptr<OnScreenKeyboardDetector> keyboard_detector_;
  base::ObserverList<InputMethodKeyboardControllerObserver, false>::Unchecked
      observers_;
  HWND hwnd_;

  // The location of TabTip.exe.
  base::string16 osk_path_;

  DISALLOW_COPY_AND_ASSIGN(OnScreenKeyboardDisplayManagerTabTip);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_ON_SCREEN_KEYBOARD_DISPLAY_MANAGER_TAP_TIP_H_

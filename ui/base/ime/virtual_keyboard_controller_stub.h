// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_
#define UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"

namespace ui {

// TODO(aluh): Rename to fake.
// This class provides a fake VirtualKeyboardController with minimal behavior.
class COMPONENT_EXPORT(UI_BASE_IME) VirtualKeyboardControllerStub final
    : public VirtualKeyboardController {
 public:
  VirtualKeyboardControllerStub();

  VirtualKeyboardControllerStub(const VirtualKeyboardControllerStub&) = delete;
  VirtualKeyboardControllerStub& operator=(
      const VirtualKeyboardControllerStub&) = delete;

  ~VirtualKeyboardControllerStub() override;

  // VirtualKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  base::ObserverList<VirtualKeyboardControllerObserver>::Unchecked observers_;
  bool visible_ = false;
};

}  // namespace ui

#endif  // UI_BASE_IME_VIRTUAL_KEYBOARD_CONTROLLER_STUB_H_

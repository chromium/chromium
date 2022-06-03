// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/virtual_keyboard_controller_stub.h"

namespace ui {

// VirtualKeyboardControllerStub member definitions.
VirtualKeyboardControllerStub::VirtualKeyboardControllerStub() {}

VirtualKeyboardControllerStub::~VirtualKeyboardControllerStub() {}

bool VirtualKeyboardControllerStub::DisplayVirtualKeyboard() {
  return false;
}

void VirtualKeyboardControllerStub::DismissVirtualKeyboard() {}

void VirtualKeyboardControllerStub::AddObserver(
    VirtualKeyboardControllerObserver* observer) {}

void VirtualKeyboardControllerStub::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {}

bool VirtualKeyboardControllerStub::IsKeyboardVisible() {
  return false;
}

}  // namespace ui

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/virtual_keyboard_controller_fuchsia.h"

#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/notreached.h"

namespace ui {

VirtualKeyboardControllerFuchsia::VirtualKeyboardControllerFuchsia(
    fuchsia::ui::input::ImeService* ime_service)
    : ime_service_(ime_service),
      ime_visibility_(
          base::ComponentContextForProcess()
              ->svc()
              ->Connect<fuchsia::ui::input::ImeVisibilityService>()) {
  DCHECK(ime_service_);

  ime_visibility_.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << " ImeVisibilityService lost.";
  });

  ime_visibility_.events().OnKeyboardVisibilityChanged = [this](bool visible) {
    keyboard_visible_ = visible;
  };
}

VirtualKeyboardControllerFuchsia::~VirtualKeyboardControllerFuchsia() = default;

bool VirtualKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  ime_service_->ShowKeyboard();
  return true;
}

void VirtualKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  ime_service_->HideKeyboard();
}

void VirtualKeyboardControllerFuchsia::AddObserver(
    VirtualKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

void VirtualKeyboardControllerFuchsia::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

bool VirtualKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

}  // namespace ui

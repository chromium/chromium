// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/input_method_manager.h"

#include "base/logging.h"

namespace chromeos {
namespace input_method {

namespace {
InputMethodManager* g_input_method_manager = nullptr;
}

InputMethodManager::State::~State() = default;

InputMethodManager::MenuItem::MenuItem() = default;

InputMethodManager::MenuItem::MenuItem(const MenuItem& other) = default;

InputMethodManager::MenuItem::~MenuItem() = default;

// static
InputMethodManager* InputMethodManager::Get() {
  return g_input_method_manager;
}

// static
void InputMethodManager::Initialize(InputMethodManager* instance) {
  DCHECK(!g_input_method_manager) << "Do not call Initialize() multiple times.";
  g_input_method_manager = instance;
}

// static
void InputMethodManager::Shutdown() {
  DCHECK(g_input_method_manager)
      << "InputMethodManager() is not initialized.";
  delete g_input_method_manager;
  g_input_method_manager = nullptr;
}

}  // namespace input_method
}  // namespace chromeos

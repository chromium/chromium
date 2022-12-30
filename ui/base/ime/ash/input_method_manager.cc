// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_manager.h"

#include "base/check.h"

namespace ash {
namespace input_method {

namespace {

InputMethodManager* g_input_method_manager = nullptr;

}  // namespace

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
  delete g_input_method_manager;
  g_input_method_manager = nullptr;
}

}  // namespace input_method
}  // namespace ash

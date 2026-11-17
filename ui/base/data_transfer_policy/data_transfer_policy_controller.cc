// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/no_destructor.h"

namespace ui {

namespace {

DataTransferPolicyController* g_base_controller = nullptr;

std::vector<DataTransferPolicyController*>& GetControllerStack() {
  static base::NoDestructor<std::vector<DataTransferPolicyController*>> stack;
  return *stack;
}

}  // namespace

// static
DataTransferPolicyController* DataTransferPolicyController::Get() {
  return g_base_controller ? g_data_transfer_policy_controller_ : nullptr;
}

// static
bool DataTransferPolicyController::HasInstance() {
  return g_base_controller != nullptr;
}

// static
void DataTransferPolicyController::DeleteInstance() {
  if (!g_base_controller) {
    return;
  }

  delete g_base_controller;
}

DataTransferPolicyController::DataTransferPolicyController() {
  auto& stack = GetControllerStack();
  if (!g_base_controller) {
    g_base_controller = this;
    stack.insert(stack.begin(), this);
  } else {
    stack.push_back(this);
  }
  g_data_transfer_policy_controller_ = stack.back();
}

DataTransferPolicyController::~DataTransferPolicyController() {
  auto& stack = GetControllerStack();
  std::erase(stack, this);
  if (g_base_controller == this) {
    g_base_controller = nullptr;
  }
  g_data_transfer_policy_controller_ =
      (g_base_controller && !stack.empty()) ? stack.back() : nullptr;
}

DataTransferPolicyController*
DataTransferPolicyController::GetParentController() const {
  const auto& stack = GetControllerStack();
  auto it = std::ranges::find(stack, this);
  if (it == stack.begin() || it == stack.end()) {
    return nullptr;
  }
  return *std::prev(it);
}

DataTransferPolicyController*
    DataTransferPolicyController::g_data_transfer_policy_controller_ = nullptr;

}  // namespace ui

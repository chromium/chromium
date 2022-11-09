// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {

// static
DataTransferPolicyController* DataTransferPolicyController::Get() {
  return g_data_transfer_policy_controller_;
}

// static
bool DataTransferPolicyController::HasInstance() {
  return g_data_transfer_policy_controller_ != nullptr;
}

// static
void DataTransferPolicyController::DeleteInstance() {
  if (!g_data_transfer_policy_controller_)
    return;

  delete g_data_transfer_policy_controller_;
  g_data_transfer_policy_controller_ = nullptr;
}

DataTransferPolicyController::DataTransferPolicyController() {
  g_data_transfer_policy_controller_ = this;
}

DataTransferPolicyController::~DataTransferPolicyController() {
  g_data_transfer_policy_controller_ = nullptr;
}

DataTransferPolicyController*
    DataTransferPolicyController::g_data_transfer_policy_controller_ = nullptr;

}  // namespace ui

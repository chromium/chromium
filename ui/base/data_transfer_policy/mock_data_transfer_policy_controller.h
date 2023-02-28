// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_
#define UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {

// A mocked data transfer policy controller for testing.
class MockDataTransferPolicyController : public DataTransferPolicyController {
 public:
  MockDataTransferPolicyController();
  MockDataTransferPolicyController(const MockDataTransferPolicyController&) =
      delete;
  MockDataTransferPolicyController& operator=(
      const MockDataTransferPolicyController&) = delete;
  ~MockDataTransferPolicyController() override;

  // DataTransferPolicyController:
  MOCK_METHOD(bool,
              IsClipboardReadAllowed,
              (const ui::DataTransferEndpoint* const data_src,
               const ui::DataTransferEndpoint* const data_dst,
               const absl::optional<size_t> size),
              (override));
  MOCK_METHOD(void,
              PasteIfAllowed,
              (const ui::DataTransferEndpoint* const data_src,
               const ui::DataTransferEndpoint* const data_dst,
               const absl::optional<size_t> size,
               content::RenderFrameHost* rfh,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              DropIfAllowed,
              (const ui::OSExchangeData* drag_data,
               const ui::DataTransferEndpoint* data_dst,
               base::OnceClosure drop_cb),
              (override));
};

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_

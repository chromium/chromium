// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_
#define UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/clipboard/file_info.h"
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
              (base::optional_ref<const ui::DataTransferEndpoint> data_src,
               base::optional_ref<const ui::DataTransferEndpoint> data_dst,
               const std::optional<size_t> size),
              (override));
  MOCK_METHOD(
      void,
      PasteIfAllowed,
      (base::optional_ref<const ui::DataTransferEndpoint> data_src,
       base::optional_ref<const ui::DataTransferEndpoint> data_dst,
       (absl::variant<size_t, std::vector<base::FilePath>> pasted_content),
       content::RenderFrameHost* rfh,
       base::OnceCallback<void(bool)> callback),
      (override));
  MOCK_METHOD(void,
              DropIfAllowed,
              (std::optional<ui::DataTransferEndpoint> data_src,
               std::optional<ui::DataTransferEndpoint> data_dst,
               std::optional<std::vector<ui::FileInfo>> filenames,
               base::OnceClosure drop_cb),
              (override));
};

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_MOCK_DATA_TRANSFER_POLICY_CONTROLLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_POLICY_CONTROLLER_H_
#define UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_POLICY_CONTROLLER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace content {
class RenderFrameHost;
}

namespace ui {

// The DataTransfer policy controller controls transferring data via
// drag-and-drop and clipboard read operations. It allows/disallows transferring
// the data given the source of the data and the destination trying to access
// the data and a set of rules controlling these source and destination.
class COMPONENT_EXPORT(UI_BASE_DATA_TRANSFER_POLICY)
    DataTransferPolicyController {
 public:
  // Returns a pointer to the existing instance of the class.
  static DataTransferPolicyController* Get();

  // Returns true if an instance exists, without forcing an initialization.
  static bool HasInstance();

  // Deletes the existing instance of the class if it's already created.
  // Indicates that restricting data transfer is no longer required.
  static void DeleteInstance();

  // nullptr can be passed instead of `data_src` or `data_dst`. If clipboard
  // read is not allowed, this function will show a notification to the user.
  virtual bool IsClipboardReadAllowed(const DataTransferEndpoint* data_src,
                                      const DataTransferEndpoint* data_dst,
                                      absl::optional<size_t> size) = 0;

  // nullptr can be passed instead of `data_src` or `data_dst`. If clipboard
  // data is set to be in warning mode, this function will show a notification
  // to the user. If clipboard read is allowed, `callback` will be invoked with
  // true. Otherwise `callback` will be invoked with false.
  // If the WebContents of `rfh` got destroyed before `callback` is invoked, the
  // notification will get closed.
  virtual void PasteIfAllowed(const DataTransferEndpoint* data_src,
                              const DataTransferEndpoint* data_dst,
                              absl::optional<size_t> size,
                              content::RenderFrameHost* rfh,
                              base::OnceCallback<void(bool)> callback) = 0;

  // nullptr can be passed instead of `data_src` or `data_dst`. If dropping the
  // data is not allowed, this function will show a notification to the user. If
  // the drop is allowed, `drop_cb` will be run. Otherwise `drop_cb` will be
  // reset.
  // `drop_cb` may be run asynchronously after the user comfirms they want to
  // drop the data.
  virtual void DropIfAllowed(const DataTransferEndpoint* data_src,
                             const DataTransferEndpoint* data_dst,
                             base::OnceClosure drop_cb) = 0;

 protected:
  DataTransferPolicyController();
  virtual ~DataTransferPolicyController();

 private:
  // A singleton of DataTransferPolicyController. Equals nullptr when there's
  // not any data transfer restrictions required.
  static DataTransferPolicyController* g_data_transfer_policy_controller_;
};

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_POLICY_CONTROLLER_H_

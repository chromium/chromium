// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_POLICY_CONTROLLER_H_
#define UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_POLICY_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/types/optional_ref.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace content {
class RenderFrameHost;
}

namespace ui {

struct FileInfo;

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

  // Returns true if `data_dst` is allowed to read clipboard data originally
  // written by `data_src`. `data_src` may be null if the clipboard data
  // originates from source can't be represented by DataTransferEndpoint;
  // similarly, `data_dst`  may be null if the data is pasted into a destination
  // can't be represented by DataTransferEndpoint e.g. Omnibox. `size` may be
  // null in some cases such as pasting files.
  virtual bool IsClipboardReadAllowed(
      base::optional_ref<const DataTransferEndpoint> data_src,
      base::optional_ref<const DataTransferEndpoint> data_dst,
      std::optional<size_t> size) = 0;

  // nullptr can be passed instead of `data_src` or `data_dst`. If clipboard
  // data is set to be in warning mode, this function will show a notification
  // to the user. If clipboard read is allowed, `callback` will be invoked with
  // true. Otherwise `callback` will be invoked with false.
  // If the WebContents of `rfh` got destroyed before `callback` is invoked, the
  // notification will get closed.
  // When pasting files, `pasted_content` contains a vector of the associated
  // pasted files. Otherwise, `pasted_content` contains the size of the pasted
  // data (text, image, etc...).
  virtual void PasteIfAllowed(
      base::optional_ref<const DataTransferEndpoint> data_src,
      base::optional_ref<const DataTransferEndpoint> data_dst,
      absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
      content::RenderFrameHost* rfh,
      base::OnceCallback<void(bool)> paste_cb) = 0;

  // nullopt can be passed instead of `data_dst` and `data_src`. If dropping
  // files, `filenames` contains the associated file info. If dropping the data
  // is not allowed, this function will show a notification to the user. If the
  // drop is allowed, `drop_cb` will be run. Otherwise `drop_cb` will be reset.
  // `drop_cb` may be run asynchronously after the user comfirms they want to
  // drop the data.
  virtual void DropIfAllowed(std::optional<DataTransferEndpoint> data_src,
                             std::optional<DataTransferEndpoint> data_dst,
                             std::optional<std::vector<FileInfo>> filenames,
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

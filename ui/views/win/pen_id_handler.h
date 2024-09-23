// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_PEN_ID_HANDLER_H_
#define UI_VIEWS_WIN_PEN_ID_HANDLER_H_

#include <combaseapi.h>
#include <stdint.h>
#include <windows.devices.input.h>
#include <windows.ui.input.h>
#include <wrl.h>

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/views_export.h"

namespace views {

// This class is responsible for retrieving the unique pen id from Windows,
// and mapping it to a unique id that will be used by Blink. When the unique
// id is fetched, the device's GUID is queried. These IDs are then mapped to
// a separate unique value, which is ultimately returned.
class VIEWS_EXPORT PenIdHandler {
 public:
  using GetPenDeviceStatics = Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Input::IPenDeviceStatics> (*)();
  class VIEWS_EXPORT [[maybe_unused, nodiscard]] ScopedPenIdStaticsForTesting {
   public:
    explicit ScopedPenIdStaticsForTesting(
        GetPenDeviceStatics pen_device_statics);
    ~ScopedPenIdStaticsForTesting();

   private:
    base::AutoReset<GetPenDeviceStatics> pen_device_resetter_;
  };

  PenIdHandler();
  virtual ~PenIdHandler();
  std::optional<int32_t> TryGetPenUniqueId(UINT32 pointer_id);

 private:
  friend class FakePenIdHandler;
  friend class FakePenIdHandlerFakeStatics;
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, GetGuidMapping);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, PenDeviceStaticsFailedToSet);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, TryGetGuidHandlesBadStatics);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, PenDeviceStaticsFailedToSet);

  // Checks if a PenDevice can be retrieved for the `pointer_id` and returns its
  // GUID if it exists.
  std::optional<std::string> TryGetGuid(UINT32 pointer_id) const;

  void InitPenIdStatics();

  base::flat_map<std::string, int32_t> guid_to_id_map_;
  int32_t current_id_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_PEN_ID_HANDLER_H_

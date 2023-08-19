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

#include <string>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/views_export.h"

namespace views {

// This class is responsible for retrieving the unique pen id from Windows,
// and mapping it to a unique id that will be used by Blink. When the unique
// id is fetched, the device's GUID is queried first. If unavailable, then
// the transducer id - which includes the transducer serial number and vendor
// id - is retrieved. These IDs are then mapped to a separate unique value,
// which is ultimately returned.
class VIEWS_EXPORT PenIdHandler {
 public:
  using GetPenDeviceStatics = Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Input::IPenDeviceStatics> (*)();
  using GetPointerPointStatics = Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::IPointerPointStatics> (*)();
  class VIEWS_EXPORT [[maybe_unused, nodiscard]] ScopedPenIdStaticsForTesting {
   public:
    explicit ScopedPenIdStaticsForTesting(
        GetPenDeviceStatics pen_device_statics,
        GetPointerPointStatics pointer_point_statics);
    ~ScopedPenIdStaticsForTesting();

   private:
    base::AutoReset<GetPenDeviceStatics> pen_device_resetter_;
    base::AutoReset<GetPointerPointStatics> pointer_point_resetter_;
  };

  PenIdHandler();
  virtual ~PenIdHandler();
  absl::optional<int32_t> TryGetPenUniqueId(UINT32 pointer_id);

 private:
  friend class FakePenIdHandler;
  friend class FakePenIdHandlerFakeStatics;
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, GetGuidMapping);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, PenDeviceStaticsFailedToSet);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, TryGetGuidHandlesBadStatics);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, PenDeviceStaticsFailedToSet);
  FRIEND_TEST_ALL_PREFIXES(PenIdHandlerTest, TryGetTransducerIdHandlesErrors);

  struct TransducerId {
    int32_t tsn = 0;
    int32_t tvid = 0;
    static constexpr int32_t kInvalidTSN = 0;
    bool operator<(const TransducerId& other) const {
      if (this->tsn != other.tsn) {
        return this->tsn < other.tsn;
      }
      return this->tvid < other.tvid;
    }
    bool operator==(const TransducerId& other) const {
      if (this->tsn == other.tsn && this->tvid == other.tvid) {
        return true;
      }
      return false;
    }
  };

  // Checks if a PenDevice can be retrieved for the `pointer_id` and returns its
  // GUID if it exists.
  absl::optional<std::string> TryGetGuid(UINT32 pointer_id) const;
  // This is a fallback scenario when TryGetGUID doesn't retrieve a PenDevice.
  // Happens when the device doesn't have both TSN/TVID (e.g.
  // SurfaceHub 1 + SurfaceHub Pen -> only has TSN, no TVID).
  TransducerId TryGetTransducerId(UINT32 pointer_id) const;

  void InitPenIdStatics();

  base::flat_map<std::string, int32_t> guid_to_id_map_;
  // Mapping from "Transducer Serial Number (TSN)" to `unique_id`. More
  // information on TSN: https://www.usb.org/sites/default/files/hut1_22.pdf
  base::flat_map<TransducerId, int32_t> transducer_id_to_id_map_;
  int32_t current_id_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_PEN_ID_HANDLER_H_

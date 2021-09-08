// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"

namespace ui {

class WaylandExchangeDataProvider final
    : public OSExchangeDataProviderNonBacked {
 public:
  WaylandExchangeDataProvider();
  WaylandExchangeDataProvider(const WaylandExchangeDataProvider&) = delete;
  WaylandExchangeDataProvider& operator=(const WaylandExchangeDataProvider&) =
      delete;
  ~WaylandExchangeDataProvider() override;

  // Builds up the mime types list corresponding to the data formats available
  // for this instance.
  std::vector<std::string> BuildMimeTypesList() const;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_SOURCE_BASE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_SOURCE_BASE_H_

#include "base/macros.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {
namespace internal {

// Implements high level (protocol-agnostic) interface to a Wayland data source.
class WaylandDataSourceBase {
 public:
  WaylandDataSourceBase();
  virtual ~WaylandDataSourceBase();

  void set_data_map(const PlatformClipboard::DataMap& data_map) {
    data_map_ = data_map;
  }

  // Writes data to the system clipboard using the protocol-defined data source.
  virtual void WriteToClipboard(const PlatformClipboard::DataMap& data_map) = 0;

 protected:
  void GetClipboardData(const std::string& mime_type,
                        base::Optional<std::vector<uint8_t>>* data) const;

 private:
  PlatformClipboard::DataMap data_map_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataSourceBase);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_INTERNAL_WAYLAND_DATA_SOURCE_BASE_H_

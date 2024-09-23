// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_HARDWARE_DISPLAY_CONTROLLER_INFO_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_HARDWARE_DISPLAY_CONTROLLER_INFO_H_

#include <stdint.h>

#include <optional>

#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"

namespace ui {

// Representation of the information required to initialize and configure a
// native display. |index| is the position of the connection and will be
// used to generate a unique identifier for the display.
class HardwareDisplayControllerInfo {
 public:
  HardwareDisplayControllerInfo(
      ScopedDrmConnectorPtr connector,
      ScopedDrmCrtcPtr crtc,
      uint8_t index,
      std::optional<display::EdidParser> edid_parser,
      std::optional<TileProperty> tile_property = std::nullopt);

  HardwareDisplayControllerInfo(const HardwareDisplayControllerInfo&) = delete;
  HardwareDisplayControllerInfo& operator=(
      const HardwareDisplayControllerInfo&) = delete;

  ~HardwareDisplayControllerInfo();

  drmModeConnector* connector() const { return connector_.get(); }
  drmModeCrtc* crtc() const { return crtc_.get(); }
  uint8_t index() const { return index_; }
  const std::optional<display::EdidParser>& edid_parser() const;
  const std::optional<TileProperty>& tile_property() const;

  void AcquireNonprimaryTileInfo(
      std::unique_ptr<HardwareDisplayControllerInfo> tile_info);

  const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
  nonprimary_tile_infos() const;

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
  nonprimary_tile_infos();

  ScopedDrmConnectorPtr ReleaseConnector() { return std::move(connector_); }

  display::DisplaySnapshot::DisplayModeList GetModesOfSize(
      const gfx::Size& size);

 private:
  ScopedDrmConnectorPtr connector_;
  ScopedDrmCrtcPtr crtc_;
  uint8_t index_;
  // This is an optional because reading the EDID can fail.
  std::optional<display::EdidParser> edid_parser_;
  // Only populated for tiled displays.
  std::optional<TileProperty> tile_property_;

  // HardwareDisplayControllerInfo of all the other tiles in the tiled display.
  // Only populated for primary tile of the tiled display.
  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
      nonprimary_tile_infos_;
};

// TODO(b/354749719): Add HardwareDisplayControllerInfoList as an alias for
// std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> after removing
// cyclic references between HardwareDisplayControllerInfo and drm_util.h.

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_HARDWARE_DISPLAY_CONTROLLER_INFO_H_

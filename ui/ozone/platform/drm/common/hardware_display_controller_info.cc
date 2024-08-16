// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"

#include <stdint.h>
#include <xf86drmMode.h>

#include <memory>
#include <optional>

#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace ui {
HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc,
    uint8_t index,
    std::optional<display::EdidParser> edid_parser,
    std::optional<TileProperty> tile_property)
    : connector_(std::move(connector)),
      crtc_(std::move(crtc)),
      index_(index),
      edid_parser_(std::move(edid_parser)),
      tile_property_(std::move(tile_property)) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() = default;

const std::optional<display::EdidParser>&
HardwareDisplayControllerInfo::edid_parser() const {
  return edid_parser_;
}
const std::optional<TileProperty>&
HardwareDisplayControllerInfo::tile_property() const {
  return tile_property_;
}

void HardwareDisplayControllerInfo::AcquireNonprimaryTileInfo(
    std::unique_ptr<HardwareDisplayControllerInfo> tile_info) {
  DCHECK(tile_info->tile_property().has_value());
  nonprimary_tile_infos_.push_back(std::move(tile_info));
}

const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
HardwareDisplayControllerInfo::nonprimary_tile_infos() const {
  return nonprimary_tile_infos_;
}

std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
HardwareDisplayControllerInfo::nonprimary_tile_infos() {
  return nonprimary_tile_infos_;
}

display::DisplaySnapshot::DisplayModeList
HardwareDisplayControllerInfo::GetModesOfSize(const gfx::Size& size) {
  display::DisplaySnapshot::DisplayModeList modes;
  for (int i = 0; i < connector_->count_modes; ++i) {
    const drmModeModeInfo& mode = connector_->modes[i];
    if (ModeSize(mode) == size) {
      modes.push_back(CreateDisplayMode(
          mode, edid_parser_ ? edid_parser_->vsync_rate_min() : std::nullopt));
    }
  }

  return modes;
}
}  // namespace ui

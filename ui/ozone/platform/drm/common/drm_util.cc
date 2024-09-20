// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"

namespace ui {

namespace {

static const size_t kDefaultCursorWidth = 64;
static const size_t kDefaultCursorHeight = 64;

bool IsCrtcInUse(
    uint32_t crtc,
    const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
        displays) {
  for (const auto& display : displays) {
    if (crtc == display->crtc()->crtc_id)
      return true;
  }

  return false;
}

// Returns a CRTC compatible with |connector| and not already used in |displays|
// and the CRTC that's currently connected to the connector.
// If there are multiple compatible CRTCs, the one that supports the majority of
// planes will be returned as best CRTC.
std::pair<uint32_t /* best_crtc */, uint32_t /* connected_crtc */> GetCrtcs(
    const DrmWrapper& drm,
    drmModeConnector* connector,
    drmModeRes* resources,
    const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>& displays,
    const std::vector<ScopedDrmPlanePtr>& planes) {
  DCHECK_GE(32, resources->count_crtcs);
  int most_crtc_planes = -1;
  uint32_t best_crtc = 0;
  uint32_t connected_crtc = 0;

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    ScopedDrmEncoderPtr encoder = drm.GetEncoder(connector->encoders[i]);
    if (!encoder)
      continue;

    if (connector->encoder_id == encoder->encoder_id)
      connected_crtc = encoder->crtc_id;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      int crtc_bit = 1 << j;
      if (!(encoder->possible_crtcs & crtc_bit) ||
          IsCrtcInUse(resources->crtcs[j], displays))
        continue;

      int supported_planes = base::ranges::count_if(
          planes, [crtc_bit](const ScopedDrmPlanePtr& p) {
            return p->possible_crtcs & crtc_bit;
          });
      if (supported_planes > most_crtc_planes ||
          (supported_planes == most_crtc_planes &&
           connected_crtc == resources->crtcs[j])) {
        most_crtc_planes = supported_planes;
        best_crtc = resources->crtcs[j];
      }
    }
  }

  return std::make_pair(best_crtc, connected_crtc);
}

// Computes the refresh rate for the specific mode. If we have enough
// information use the mode timings to compute a more exact value otherwise
// fallback to using the mode's vertical refresh rate (the kernel computes this
// the same way, however there is a loss in precision since |vrefresh| is sent
// as an integer).
float GetRefreshRate(const drmModeModeInfo& mode) {
  if (!mode.htotal || !mode.vtotal)
    return mode.vrefresh;

  float clock = mode.clock;
  float htotal = mode.htotal;
  float vtotal = mode.vtotal;

  return (clock * 1000.0f) / (htotal * vtotal);
}

display::DisplayConnectionType GetDisplayConnectionType(
    drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return display::DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return display::DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_DSI:
      return display::DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return display::DISPLAY_CONNECTION_TYPE_HDMI;
    case DRM_MODE_CONNECTOR_VIRTUAL:
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDRMVirtualConnectorIsExternal)) {
        return display::DISPLAY_CONNECTION_TYPE_UNKNOWN;
      }
      // A display on VM is treated as an internal display unless flag
      // --drm-virtual-connector-is-external is present.
      return display::DISPLAY_CONNECTION_TYPE_INTERNAL;
    default:
      return display::DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

template <typename T>
int GetDrmProperty(const DrmWrapper& drm,
                   T* object,
                   const std::string& name,
                   ScopedDrmPropertyPtr* property) {
  for (uint32_t i = 0; i < static_cast<uint32_t>(object->count_props); ++i) {
    ScopedDrmPropertyPtr tmp = drm.GetProperty(object->props[i]);
    if (!tmp)
      continue;

    if (name == tmp->name) {
      *property = std::move(tmp);
      return i;
    }
  }

  return -1;
}

std::string GetNameForEnumValue(drmModePropertyRes* property, uint32_t value) {
  for (int i = 0; i < property->count_enums; ++i) {
    if (property->enums[i].value == value)
      return property->enums[i].name;
  }

  return std::string();
}

ScopedDrmPropertyBlobPtr GetDrmPropertyBlob(const DrmWrapper& drm,
                                            drmModeConnector* connector,
                                            const std::string& name) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(drm, connector, name, &property);
  if (index < 0)
    return nullptr;

  if (property->flags & DRM_MODE_PROP_BLOB) {
    return drm.GetPropertyBlob(connector->prop_values[index]);
  }

  return nullptr;
}

display::PrivacyScreenState GetPrivacyScreenState(const DrmWrapper& drm,
                                                  drmModeConnector* connector) {
  ScopedDrmPropertyPtr sw_property;
  const int sw_index = GetDrmProperty(
      drm, connector, kPrivacyScreenSwStatePropertyName, &sw_property);
  ScopedDrmPropertyPtr hw_property;
  const int hw_index = GetDrmProperty(
      drm, connector, kPrivacyScreenHwStatePropertyName, &hw_property);

  // Both privacy-screen properties (software- and hardware-state) must be
  // present in order for the feature to be supported, but the hardware-state
  // property indicates the true state of the privacy screen.
  if (sw_index >= 0 && hw_index >= 0) {
    const std::string hw_enum_value = GetNameForEnumValue(
        hw_property.get(), connector->prop_values[hw_index]);
    const display::PrivacyScreenState* state =
        GetInternalTypeValueFromDrmEnum(hw_enum_value, kPrivacyScreenStates);
    return state ? *state : display::kNotSupported;
  }

  // If the new privacy screen UAPI properties are missing, try to fetch the
  // legacy privacy screen property.
  ScopedDrmPropertyPtr legacy_property;
  const int legacy_index = GetDrmProperty(
      drm, connector, kPrivacyScreenPropertyNameLegacy, &legacy_property);
  if (legacy_index >= 0) {
    const std::string legacy_enum_value = GetNameForEnumValue(
        legacy_property.get(), connector->prop_values[legacy_index]);
    const display::PrivacyScreenState* state = GetInternalTypeValueFromDrmEnum(
        legacy_enum_value, kPrivacyScreenStates);
    return state ? *state : display::kNotSupported;
  }

  return display::PrivacyScreenState::kNotSupported;
}

bool HasContentProtectionKey(const DrmWrapper& drm,
                             drmModeConnector* connector) {
  ScopedDrmPropertyPtr content_protection_key_property;
  int idx = GetDrmProperty(drm, connector, kContentProtectionKey,
                           &content_protection_key_property);
  return idx > -1;
}

std::vector<uint64_t> GetPathTopology(const DrmWrapper& drm,
                                      drmModeConnector* connector) {
  ScopedDrmPropertyBlobPtr path_blob = drm.GetPropertyBlob(connector, "PATH");

  if (!path_blob) {
    DCHECK_GT(connector->connector_id, 0u);

    // The topology is consisted solely of the connector id.
    return {base::strict_cast<uint64_t>(connector->connector_id)};
  }

  return ParsePathBlob(*path_blob);
}

bool IsAspectPreserving(const DrmWrapper& drm, drmModeConnector* connector) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(drm, connector, "scaling mode", &property);
  if (index < 0)
    return false;

  return (GetNameForEnumValue(property.get(), connector->prop_values[index]) ==
          "Full aspect");
}

std::optional<TileProperty> GetTileProperty(
    const DrmWrapper& drm,
    const std::optional<display::EdidParser>& edid_parser,
    drmModeConnector* connector) {
  const ScopedDrmPropertyBlobPtr tile_blob =
      drm.GetPropertyBlob(connector, "TILE");
  if (!tile_blob) {
    return std::nullopt;
  }

  std::optional<TileProperty> tile_property = ParseTileBlob(*tile_blob);
  if (!tile_property.has_value()) {
    return std::nullopt;
  }

  if (edid_parser.has_value()) {
    tile_property->scale_to_fit_display = edid_parser->TileCanScaleToFit();
  }

  return tile_property;
}

display::PanelOrientation GetPanelOrientation(const DrmWrapper& drm,
                                              drmModeConnector* connector) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(drm, connector, "panel orientation", &property);
  if (index < 0)
    return display::PanelOrientation::kNormal;

  // If the DRM driver doesn't provide panel orientation then this property
  // will be DRM_MODE_PANEL_ORIENTATION_UNKNOWN (which is -1, except
  // `prop_values` is unsigned, so compare against max uint64_t). Assume that
  // panels with unknown orientation have normal orientation.
  if (connector->prop_values[index] == std::numeric_limits<uint64_t>::max())
    return display::PanelOrientation::kNormal;

  DCHECK_LE(connector->prop_values[index], display::PanelOrientation::kLast);
  return static_cast<display::PanelOrientation>(connector->prop_values[index]);
}

// Read a file and trim whitespace. If the file can't be read, returns
// nullopt.
std::optional<std::string> ReadFileAndTrim(const base::FilePath& path) {
  std::string data;
  if (!base::ReadFileToString(path, &data))
    return std::nullopt;

  return std::string(
      base::TrimWhitespaceASCII(data, base::TrimPositions::TRIM_ALL));
}

// Sort modes in |modes_in_out| from largest to smallest as defined by
// DisplayMode::operator>().
void SortDisplayModeListDesc(
    display::DisplaySnapshot::DisplayModeList& modes_in_out) {
  std::stable_sort(
      modes_in_out.begin(), modes_in_out.end(),
      [](const std::unique_ptr<const display::DisplayMode>& left,
         const std::unique_ptr<const display::DisplayMode>& right) {
        return *left > *right;
      });
}

// Given all |tiled_infos| belonging to the same display, select the "primary"
// tile that will represent all the tiles. Primary tile is the only active tile
// if the display is configured with a non-tile mode.
const HardwareDisplayControllerInfo* GetPrimaryTileInfo(
    const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
        tiled_infos) {
  if (tiled_infos.empty()) {
    return nullptr;
  }
  // 1. Filter for tile switch scale to fit capability
  std::vector<const HardwareDisplayControllerInfo*> scalable_tiles,
      unscalable_tiles;
  for (const auto& tiled_info : tiled_infos) {
    const HardwareDisplayControllerInfo* tiled_info_ptr = tiled_info.get();
    if (tiled_info_ptr->tile_property()->scale_to_fit_display) {
      scalable_tiles.push_back(tiled_info_ptr);
    } else {
      unscalable_tiles.push_back(tiled_info_ptr);
    }
  }

  if (scalable_tiles.size() == 1) {
    return scalable_tiles.front();
  }

  // If there were multiple tiles that could have scaled, then use those for
  // round 2. Only if there were no tiles capable of scaling should we consider
  // all the tiles for round 2.
  std::vector<const HardwareDisplayControllerInfo*> primary_eligible_tiles;
  if (!scalable_tiles.empty()) {
    primary_eligible_tiles = std::move(scalable_tiles);
  } else {
    primary_eligible_tiles = std::move(unscalable_tiles);
  }

  // 2. The tile with the most # of modes should be the primary.
  std::vector<const HardwareDisplayControllerInfo*> max_mode_tiles;
  int max_num_modes = -1;
  for (const auto* tiled_info : primary_eligible_tiles) {
    const int num_modes = tiled_info->connector()->count_modes;
    if (num_modes > max_num_modes) {
      max_num_modes = num_modes;
      max_mode_tiles = {tiled_info};
    } else if (num_modes == max_num_modes) {
      max_mode_tiles.push_back(tiled_info);
    }
  }

  if (max_mode_tiles.size() == 1) {
    return max_mode_tiles.front();
  }

  // 3. Break ties by taking the tile with TileProperty::location closest to the
  // origin. Breaking ties deterministically keeps EDID-based display IDs
  // stable.
  primary_eligible_tiles = std::move(max_mode_tiles);
  const HardwareDisplayControllerInfo* tile_closest_to_origin =
      primary_eligible_tiles.front();
  gfx::Point closest_point = tile_closest_to_origin->tile_property()->location;
  for (const auto* tile : primary_eligible_tiles) {
    const gfx::Point& tile_location = tile->tile_property()->location;
    if (tile_location < closest_point) {
      closest_point = tile_location;
      tile_closest_to_origin = tile;
    }
  }

  return tile_closest_to_origin;
}
bool ContainsModePtr(const display::DisplaySnapshot::DisplayModeList& modes,
                     const display::DisplayMode* target_mode) {
  for (const auto& mode : modes) {
    if (mode.get() == target_mode) {
      return true;
    }
  }
  return false;
}

bool ContainsModeEq(const display::DisplaySnapshot::DisplayModeList& modes,
                    const display::DisplayMode& target_mode) {
  for (const auto& mode : modes) {
    if (*mode == target_mode) {
      return true;
    }
  }
  return false;
}

// Prune all tile modes in |primary_tile_modes_in_out| that doesn't show up in
// all other tiles in the tiled display.
void PruneTileModesNotPresentInAllTiles(
    const HardwareDisplayControllerInfo& primary_tile_info,
    display::DisplaySnapshot::DisplayModeList& primary_tile_modes_in_out) {
  const std::optional<TileProperty>& primary_tile_property =
      primary_tile_info.tile_property();
  if (!primary_tile_property.has_value()) {
    return;
  }

  const gfx::Size& tile_size = primary_tile_property->tile_size;
  for (auto primary_tile_mode_it = primary_tile_modes_in_out.begin();
       primary_tile_mode_it != primary_tile_modes_in_out.end();) {
    // Skip non-tile modes.
    if (!(*primary_tile_mode_it) ||
        (*primary_tile_mode_it)->size() != tile_size) {
      ++primary_tile_mode_it;
      continue;
    }

    bool mode_found_in_all_tiles = true;
    for (const auto& nonprimary_tile_info :
         primary_tile_info.nonprimary_tile_infos()) {
      const display::DisplaySnapshot::DisplayModeList nonprimary_tile_modes =
          nonprimary_tile_info->GetModesOfSize(tile_size);
      if (!ContainsModeEq(nonprimary_tile_modes, **primary_tile_mode_it)) {
        mode_found_in_all_tiles = false;
        break;
      }
    }

    if (mode_found_in_all_tiles) {
      ++primary_tile_mode_it;
    } else {
      primary_tile_mode_it =
          primary_tile_modes_in_out.erase(primary_tile_mode_it);
    }
  }
}

// Prune all tile modes in |modes_in_out| if all tiles in a display are not
// connected to prevent the display from having blank tiles.
void PruneTileModesForIncompleteGroup(
    const HardwareDisplayControllerInfo& tiled_display_info,
    display::DisplaySnapshot::DisplayModeList& modes_in_out) {
  const std::optional<TileProperty>& tile_property =
      tiled_display_info.tile_property();
  if (!tile_property.has_value()) {
    return;
  }

  const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
      nonprimary_tiles = tiled_display_info.nonprimary_tile_infos();
  // Prune all tile modes if not all tiles in the display are connected yet.
  if (tile_property->tile_layout.GetArea() !=
      static_cast<int>(nonprimary_tiles.size()) + 1) {
    modes_in_out.erase(
        std::remove_if(
            modes_in_out.begin(), modes_in_out.end(),
            [&tile_property](
                const std::unique_ptr<const display::DisplayMode>& mode) {
              return mode->size() == tile_property->tile_size;
            }),
        modes_in_out.end());
    return;
  }
}

// Replaces all tile modes with the full tile composited mode.
// Note that individual tiles in a tiled display advertise modes with size of
// the tile instead of the full display.
void ConvertTileModesToCompositedModes(
    const HardwareDisplayControllerInfo& tiled_display_info,
    display::DisplaySnapshot::DisplayModeList& modes_in_out,
    const display::DisplayMode*& current_mode_out,
    const display::DisplayMode*& native_mode_out) {
  const std::optional<TileProperty>& tile_property =
      tiled_display_info.tile_property();
  if (!tile_property.has_value()) {
    return;
  }
  // For every mode with same resolution as the tile size, replace with a a new,
  // equivalent mode with the full tile-composited display resolution.
  for (auto& mode : modes_in_out) {
    if (mode->size() != tile_property->tile_size) {
      continue;
    }

    std::unique_ptr<display::DisplayMode> tile_mode =
        mode->CopyWithSize(GetTotalTileDisplaySize(*tile_property));
    if (current_mode_out == mode.get()) {
      current_mode_out = tile_mode.get();
    }
    if (native_mode_out == mode.get()) {
      native_mode_out = tile_mode.get();
    }

    mode = std::move(tile_mode);
  }

  SortDisplayModeListDesc(modes_in_out);
}

std::unique_ptr<HardwareDisplayControllerInfo> PopPrimaryTileInfo(
    const HardwareDisplayControllerInfo* primary_tile_info_ptr,
    std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>& infos) {
  std::unique_ptr<HardwareDisplayControllerInfo> primary_tile_info;
  for (auto tile_info = infos.begin(); tile_info != infos.end(); tile_info++) {
    if (tile_info->get() == primary_tile_info_ptr) {
      primary_tile_info = std::move(*tile_info);
      infos.erase(tile_info);
      break;
    }
  }
  return primary_tile_info;
}
}  // namespace

ScopedDrmPropertyPtr FindDrmProperty(const DrmWrapper& drm,
                                     drmModeObjectProperties* properties,
                                     const char* name) {
  for (uint32_t i = 0; i < properties->count_props; ++i) {
    ScopedDrmPropertyPtr property = drm.GetProperty(properties->props[i]);
    if (property && !strcmp(property->name, name))
      return property;
  }
  return nullptr;
}

bool HasColorCorrectionMatrix(const DrmWrapper& drm, drmModeCrtc* crtc) {
  ScopedDrmObjectPropertyPtr crtc_props =
      drm.GetObjectProperties(crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
  return !!FindDrmProperty(drm, crtc_props.get(), "CTM");
}

const gfx::Size ModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

float ModeRefreshRate(const drmModeModeInfo& mode) {
  return GetRefreshRate(mode);
}

bool ModeIsInterlaced(const drmModeModeInfo& mode) {
  return mode.flags & DRM_MODE_FLAG_INTERLACE;
}

const std::optional<float> ModeVSyncRateMin(
    const drmModeModeInfo& mode,
    const std::optional<uint16_t>& vsync_rate_min_from_edid) {
  if (!vsync_rate_min_from_edid.has_value() ||
      vsync_rate_min_from_edid.value() == 0) {
    return std::nullopt;
  }

  if (!mode.htotal) {
    return vsync_rate_min_from_edid;
  }

  float clock_hz = mode.clock * 1000.0f;
  float htotal = mode.htotal;

  // Calculate the vtotal from the imprecise min vsync rate.
  float vtotal_extended =
      clock_hz / (htotal * vsync_rate_min_from_edid.value());
  // Clamp the calculated vtotal and determine the precise min vsync rate.
  return clock_hz / (htotal * std::floor(vtotal_extended));
}

gfx::Size GetMaximumCursorSize(const DrmWrapper& drm) {
  uint64_t width = 0, height = 0;
  // Querying cursor dimensions is optional and is unsupported on older Chrome
  // OS kernels.
  if (!drm.GetCapability(DRM_CAP_CURSOR_WIDTH, &width) ||
      !drm.GetCapability(DRM_CAP_CURSOR_HEIGHT, &height)) {
    return gfx::Size(kDefaultCursorWidth, kDefaultCursorHeight);
  }
  return gfx::Size(width, height);
}

bool IsVrrCapable(const DrmWrapper& drm, drmModeConnector* connector) {
  ScopedDrmPropertyPtr vrr_capable_property;
  const int vrr_capable_index = GetDrmProperty(
      drm, connector, kVrrCapablePropertyName, &vrr_capable_property);
  return vrr_capable_index >= 0 && connector->prop_values[vrr_capable_index];
}

bool IsVrrEnabled(const DrmWrapper& drm, drmModeCrtc* crtc) {
  ScopedDrmObjectPropertyPtr crtc_props =
      drm.GetObjectProperties(crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
  ScopedDrmPropertyPtr vrr_enabled_property;
  const int vrr_enabled_index = GetDrmProperty(
      drm, crtc_props.get(), kVrrEnabledPropertyName, &vrr_enabled_property);
  return vrr_enabled_index >= 0 && crtc_props->prop_values[vrr_enabled_index];
}

display::VariableRefreshRateState GetVariableRefreshRateState(
    const DrmWrapper& drm,
    HardwareDisplayControllerInfo* info) {
  if (!IsVrrCapable(drm, info->connector())) {
    return display::VariableRefreshRateState::kVrrNotCapable;
  }
  if (!info->edid_parser()->vsync_rate_min().has_value() ||
      info->edid_parser()->vsync_rate_min().value() == 0) {
    return display::VariableRefreshRateState::kVrrNotCapable;
  }

  if (IsVrrEnabled(drm, info->crtc())) {
    return display::VariableRefreshRateState::kVrrEnabled;
  }

  return display::VariableRefreshRateState::kVrrDisabled;
}

std::pair<std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>,
          std::vector<uint32_t>>
GetDisplayInfosAndInvalidCrtcs(const DrmWrapper& drm) {
  ScopedDrmResourcesPtr resources = drm.GetResources();
  DCHECK(resources) << "Failed to get DRM resources";
  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> displays;
  std::vector<uint32_t> invalid_crtcs;

  std::vector<ScopedDrmConnectorPtr> connectors;
  std::vector<drmModeConnector*> available_connectors;
  const size_t count_connectors = resources->count_connectors;
  for (size_t i = 0; i < count_connectors; ++i) {
    if (i >= kMaxDrmConnectors) {
      LOG(WARNING) << "Reached the current limit of " << kMaxDrmConnectors
                   << " connectors per DRM. Ignoring the remaining "
                   << count_connectors - kMaxDrmConnectors << " connectors.";
      break;
    }

    ScopedDrmConnectorPtr connector =
        drm.GetConnector(resources->connectors[i]);
    // In case of zombie connectors, verify that the connector is valid by
    // checking if it has props.
    // Zombie connectors can occur when an MST (which creates a new connector ID
    // upon connection) is disconnected but the kernel hasn't cleaned up the old
    // connector ID yet.
    if (!connector || !drm.GetObjectProperties(resources->connectors[i],
                                               DRM_MODE_OBJECT_CONNECTOR)) {
      continue;
    }

    if (connector->connection == DRM_MODE_CONNECTED) {
      if (connector->count_modes != 0) {
        available_connectors.push_back(connector.get());
      } else {
        LOG(WARNING) << "[CONNECTOR:" << connector->connector_id
                     << "] is connected but has no modes. Connector ignored.";
      }
    }

    connectors.emplace_back(std::move(connector));
  }

  base::flat_map<drmModeConnector*, int> connector_crtcs;
  for (auto* connector : available_connectors) {
    std::vector<uint32_t> encoder_ids(
        connector->encoders, connector->encoders + connector->count_encoders);
    connector_crtcs[connector] =
        GetPossibleCrtcsBitmaskFromEncoders(drm, encoder_ids);
  }
  // Make sure to start assigning a crtc to the connector that supports the
  // fewest crtcs first.
  std::stable_sort(available_connectors.begin(), available_connectors.end(),
                   [&connector_crtcs](drmModeConnector* const c1,
                                      drmModeConnector* const c2) {
                     // When c1 supports a proper subset of the crtcs of c2, we
                     // should process c1 first (return true).
                     int c1_crtcs = connector_crtcs[c1];
                     int c2_crtcs = connector_crtcs[c2];
                     return (c1_crtcs & c2_crtcs) == c1_crtcs &&
                            c1_crtcs != c2_crtcs;
                   });

  ScopedDrmPlaneResPtr plane_resources = drm.GetPlaneResources();
  std::vector<ScopedDrmPlanePtr> planes;
  for (uint32_t i = 0; i < plane_resources->count_planes; i++)
    planes.emplace_back(drm.GetPlane(plane_resources->planes[i]));

  for (auto* c : available_connectors) {
    uint32_t best_crtc, connected_crtc;
    std::tie(best_crtc, connected_crtc) =
        GetCrtcs(drm, c, resources.get(), displays, planes);
    if (!best_crtc)
      continue;

    // If the currently connected CRTC isn't the best CRTC for the connector,
    // add the CRTC to the list of Invalid CRTCs.
    if (connected_crtc && connected_crtc != best_crtc)
      invalid_crtcs.push_back((connected_crtc));

    ScopedDrmCrtcPtr crtc = drm.GetCrtc(best_crtc);
    auto connector_iter =
        base::ranges::find(connectors, c, &ScopedDrmConnectorPtr::get);
    CHECK(connector_iter != connectors.end(), base::NotFatalUntil::M130);
    // |connectors.size()| <= 256, so |index| should be between 0-255.
    const uint8_t index = connector_iter - connectors.begin();
    DCHECK_LT(index, connectors.size());

    drmModeConnector* connector = connector_iter->get();
    ScopedDrmPropertyBlobPtr edid_blob(
        GetDrmPropertyBlob(drm, connector, "EDID"));
    std::optional<display::EdidParser> edid_parser;
    if (edid_blob) {
      uint8_t* edid_blob_ptr = static_cast<uint8_t*>(edid_blob->data);
      std::vector<uint8_t> edid(edid_blob_ptr,
                                edid_blob_ptr + edid_blob->length);
      const bool is_external = GetDisplayConnectionType(connector) !=
                               display::DISPLAY_CONNECTION_TYPE_INTERNAL;
      edid_parser = display::EdidParser(std::move(edid), is_external);
    } else {
      VLOG(1) << "Failed to get EDID blob for connector "
              << connector->connector_id;
    }

    std::optional<TileProperty> tile_property;
    if (display::features::IsTiledDisplaySupportEnabled()) {
      tile_property = GetTileProperty(drm, edid_parser, connector);
    }

    displays.push_back(std::make_unique<HardwareDisplayControllerInfo>(
        std::move(*connector_iter), std::move(crtc), index,
        std::move(edid_parser), std::move(tile_property)));
  }

  return std::make_pair(std::move(displays), std::move(invalid_crtcs));
}

std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
GetAvailableDisplayControllerInfos(const DrmWrapper& drm) {
  return GetDisplayInfosAndInvalidCrtcs(drm).first;
}

uint32_t GetPossibleCrtcsBitmaskFromEncoders(
    const DrmWrapper& drm,
    const std::vector<uint32_t>& encoder_ids) {
  uint32_t possible_crtcs = 0;
  for (uint32_t encoder_id : encoder_ids) {
    ScopedDrmEncoderPtr encoder = drm.GetEncoder(encoder_id);
    if (!encoder) {
      continue;
    }
    possible_crtcs |= encoder->possible_crtcs;
  }

  return possible_crtcs;
}

std::vector<uint32_t> GetPossibleCrtcIdsFromBitmask(
    const DrmWrapper& drm,
    const uint32_t possible_crtcs_bitmask) {
  std::vector<uint32_t> crtcs;
  ScopedDrmResourcesPtr resources = drm.GetResources();
  for (int i = 0; i < resources->count_crtcs; i++) {
    // CRTC mask of |possible_crtcs_bitmask| is just 1 offset by the index in
    // drm_crtc_index().
    const uint32_t current_crtc_mask = 1 << i;
    if (possible_crtcs_bitmask & current_crtc_mask) {
      crtcs.push_back(resources->crtcs[i]);
    }
  }

  return crtcs;
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock && lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay && lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start && lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal && lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start && lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal && lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags && strcmp(lhs.name, rhs.name) == 0;
}

std::unique_ptr<display::DisplayMode> CreateDisplayMode(
    const drmModeModeInfo& mode,
    const std::optional<uint16_t>& vsync_rate_min_from_edid) {
  return std::make_unique<display::DisplayMode>(
      gfx::Size{mode.hdisplay, mode.vdisplay},
      mode.flags & DRM_MODE_FLAG_INTERLACE, GetRefreshRate(mode),
      ModeVSyncRateMin(mode, vsync_rate_min_from_edid));
}

std::unique_ptr<drmModeModeInfo> CreateVirtualMode(
    const drmModeModeInfo& base_mode,
    float virtual_refresh_rate) {
  if (!base_mode.htotal) {
    return nullptr;
  }

  float clock_hz = base_mode.clock * 1000.0f;
  float htotal = base_mode.htotal;

  uint16_t virtual_vtotal =
      std::round(clock_hz / (htotal * virtual_refresh_rate));
  // Vtotal can only be increased from the base mode because virtual modes rely
  // on VRR capabilities (i.e. the back porch can be extended but not
  // diminished).
  if (virtual_vtotal < base_mode.vtotal) {
    return nullptr;
  }

  auto out_mode = std::make_unique<drmModeModeInfo>();
  *out_mode = base_mode;
  out_mode->vtotal = virtual_vtotal;
  return out_mode;
}

display::DisplaySnapshot::DisplayModeList ExtractDisplayModes(
    HardwareDisplayControllerInfo* info,
    const gfx::Size& active_pixel_size,
    const display::DisplayMode** out_current_mode,
    const display::DisplayMode** out_native_mode) {
  DCHECK(out_current_mode);
  DCHECK(out_native_mode);

  *out_current_mode = nullptr;
  *out_native_mode = nullptr;
  display::DisplaySnapshot::DisplayModeList modes;
  for (int i = 0; i < info->connector()->count_modes; ++i) {
    const drmModeModeInfo& mode = info->connector()->modes[i];
    modes.push_back(CreateDisplayMode(
        mode, info->edid_parser() ? info->edid_parser()->vsync_rate_min()
                                  : std::nullopt));

    if (info->crtc()->mode_valid && SameMode(info->crtc()->mode, mode))
      *out_current_mode = modes.back().get();

    if (mode.type & DRM_MODE_TYPE_PREFERRED) {
      if (*out_native_mode == nullptr) {
        *out_native_mode = modes.back().get();
      } else {
        LOG(WARNING) << "Found more than one preferred modes. The first one "
                        "will be used.";
      }
    }
  }

  // If we couldn't find a preferred mode, then try to find a mode that has the
  // same size as the first detailed timing descriptor in the EDID.
  if (!*out_native_mode && !active_pixel_size.IsEmpty()) {
    for (const auto& mode : modes) {
      if (mode->size() == active_pixel_size) {
        *out_native_mode = mode.get();
        break;
      }
    }
  }

  // If we still have no preferred mode, then use the first one since it should
  // be the best mode.
  if (!*out_native_mode && !modes.empty())
    *out_native_mode = modes.front().get();

  return modes;
}

std::unique_ptr<display::DisplaySnapshot> CreateDisplaySnapshot(
    const DrmWrapper& drm,
    HardwareDisplayControllerInfo* info,
    uint8_t device_index) {
  const uint8_t display_index =
      display::ConnectorIndex8(device_index, info->index());
  const uint16_t connector_index =
      display::ConnectorIndex16(device_index, info->index());
  const gfx::Size physical_size =
      gfx::Size(info->connector()->mmWidth, info->connector()->mmHeight);
  const display::DisplayConnectionType type =
      GetDisplayConnectionType(info->connector());
  uint64_t base_connector_id = 0u;
  std::vector<uint64_t> path_topology = GetPathTopology(drm, info->connector());
  if (!path_topology.empty()) {
    base_connector_id = path_topology.front();
    path_topology.erase(path_topology.begin());
  }
  const bool is_aspect_preserving_scaling =
      IsAspectPreserving(drm, info->connector());
  const display::PanelOrientation panel_orientation =
      GetPanelOrientation(drm, info->connector());
  const display::PrivacyScreenState privacy_screen_state =
      GetPrivacyScreenState(drm, info->connector());
  const bool has_content_protection_key =
      HasContentProtectionKey(drm, info->connector());
  display::DisplaySnapshot::ColorInfo color_info;
  color_info.supports_color_temperature_adjustment =
      HasColorCorrectionMatrix(drm, info->crtc());
  const gfx::Size maximum_cursor_size = GetMaximumCursorSize(drm);
  const display::VariableRefreshRateState variable_refresh_rate_state =
      GetVariableRefreshRateState(drm, info);

  std::string display_name;
  // Make sure the ID contains non index part.
  int64_t port_display_id = display_index | 0x100;
  int64_t edid_display_id = port_display_id;
  int64_t product_code = display::DisplaySnapshot::kInvalidProductCode;
  int32_t year_of_manufacture = display::kInvalidYearOfManufacture;
  bool has_overscan = false;
  color_info.bits_per_channel = 8u;
  // Active pixels size from the first detailed timing descriptor in the EDID.
  gfx::Size active_pixel_size;

  const std::optional<display::EdidParser>& edid_parser = info->edid_parser();
  base::UmaHistogramBoolean("DrmUtil.CreateDisplaySnapshot.HasEdidBlob",
                            edid_parser.has_value());
  const std::vector<uint8_t>& edid = edid_parser.has_value()
                                         ? edid_parser->edid_blob()
                                         : std::vector<uint8_t>();
  if (edid_parser.has_value()) {
    display_name = edid_parser->display_name();
    active_pixel_size = edid_parser->active_pixel_size();
    product_code = edid_parser->GetProductCode();
    port_display_id = edid_parser->GetIndexBasedDisplayId(display_index);
    edid_display_id = edid_parser->GetEdidBasedDisplayId();
    year_of_manufacture = edid_parser->year_of_manufacture();
    has_overscan =
        edid_parser->has_overscan_flag() && edid_parser->overscan_flag();
    color_info.color_space = display::GetColorSpaceFromEdid(*edid_parser);
    // Populate the EDID primaries and gamma from the gfx::ColorSpace.
    // TODO(crbug.com/40945652): Extract this directly.
    if (auto sk_color_space = color_info.color_space.ToSkColorSpace()) {
      skcms_TransferFunction fn;
      skcms_Matrix3x3 to_xyzd50;
      sk_color_space->toXYZD50(&to_xyzd50);
      sk_color_space->transferFn(&fn);
      color_info.edid_primaries =
          skia::GetD65PrimariesFromToXYZD50Matrix(to_xyzd50);
      color_info.edid_gamma = fn.g;
    }
    base::UmaHistogramBoolean("DrmUtil.CreateDisplaySnapshot.IsHDR",
                              color_info.color_space.IsHDR());
    color_info.bits_per_channel = std::max(edid_parser->bits_per_channel(), 0);
    base::UmaHistogramCounts100("DrmUtil.CreateDisplaySnapshot.BitsPerChannel",
                                color_info.bits_per_channel);
    color_info.hdr_static_metadata = edid_parser->hdr_static_metadata();
  }

  const display::DisplayMode* current_mode = nullptr;
  const display::DisplayMode* native_mode = nullptr;
  display::DisplaySnapshot::DisplayModeList modes =
      ExtractDisplayModes(info, active_pixel_size, &current_mode, &native_mode);

  const display::DrmFormatsAndModifiers drm_formats_and_modifiers =
      drm.GetFormatsAndModifiersForCrtc(info->crtc()->crtc_id);

  if (info->tile_property().has_value()) {
    PruneTileModesForIncompleteGroup(*info, modes);
    PruneTileModesNotPresentInAllTiles(*info, modes);
    ConvertTileModesToCompositedModes(*info, modes, current_mode, native_mode);

    if (!ContainsModePtr(modes, native_mode)) {
      // Fall back to first mode in |modes|.
      native_mode = modes.front().get();
    }

    if (!ContainsModePtr(modes, current_mode)) {
      // Fall back to using |native_mode|.
      current_mode = native_mode;
    }
  }

  return std::make_unique<display::DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      gfx::Point(), physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, color_info, display_name, drm.device_path(),
      std::move(modes), panel_orientation, edid, current_mode, native_mode,
      product_code, year_of_manufacture, maximum_cursor_size,
      variable_refresh_rate_state, drm_formats_and_modifiers);
}

int GetFourCCFormatForOpaqueFramebuffer(gfx::BufferFormat format) {
  // DRM atomic interface doesn't currently support specifying an alpha
  // blending. We can simulate disabling alpha blending creating an fb
  // with a format without the alpha channel.
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGRA_1010102:
      return DRM_FORMAT_XRGB2101010;
    case gfx::BufferFormat::RGBA_1010102:
      return DRM_FORMAT_XBGR2101010;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::P010:
      return DRM_FORMAT_P010;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

const char* GetNameForColorspace(const gfx::ColorSpace color_space) {
  if (color_space == gfx::ColorSpace::CreateHDR10())
    return kColorSpaceBT2020RGBEnumName;

  return kColorSpaceDefaultEnumName;
}

uint64_t GetEnumValueForName(const DrmWrapper& drm,
                             int property_id,
                             const char* str) {
  ScopedDrmPropertyPtr res = drm.GetProperty(property_id);
  for (int i = 0; i < res->count_enums; ++i) {
    if (strcmp(res->enums[i].name, str) == 0) {
      return res->enums[i].value;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool IsTileMode(const gfx::Size mode_size, const TileProperty& tile_property) {
  return mode_size == tile_property.tile_size;
}

const gfx::Point GetTileCrtcOffset(const TileProperty& tiled_property) {
  return gfx::Point(
      tiled_property.location.x() * tiled_property.tile_size.width(),
      tiled_property.location.y() * tiled_property.tile_size.height());
}

// Returns a vector that holds the path topology of the display. Returns an
// empty vector upon failure.
//
// A path topology c-string is of the format:
//    mst:{DRM_BASE_CONNECTOR_ID#}-{BRANCH_1_PORT#}-...-{BRANCH_N_PORT#}\0
//
// For example, the display configuration:
//    Device <--conn6-- MST1 <--port2-- MST2 <--port1-- Display
// may produce the following topology c-string:
//     "mst:6-2-1"
//
// To see how this string is constructed in the DRM:
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/gpu/drm/drm_dp_mst_topology.c?h=v5.10-rc3#n2229
std::vector<uint64_t> ParsePathBlob(const drmModePropertyBlobRes& path_blob) {
  if (!path_blob.length) {
    LOG(ERROR) << "PATH property blob is empty.";
    return {};
  }

  std::string path_str(
      static_cast<char*>(path_blob.data),
      base::strict_cast<std::string::size_type>(path_blob.length));
  std::string_view path_string_piece(path_str);
  path_string_piece = base::TrimString(path_string_piece, std::string("\0", 1u),
                                       base::TRIM_TRAILING);

  const std::string prefix("mst:");
  if (!base::StartsWith(path_string_piece, prefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Invalid PATH string prefix. Does not contain '" << prefix
               << "'. Input: '" << path_str << "'";
    return {};
  }
  path_string_piece.remove_prefix(prefix.length());

  std::vector<uint64_t> path;
  for (const auto& string_port :
       base::SplitStringPiece(path_string_piece, "-", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    uint64_t int_port = 0;
    if (base::StringToUint64(string_port, &int_port) && int_port > 0) {
      path.push_back(int_port);
    } else {
      LOG(ERROR)
          << "One or more port values in the PATH string are invalid. Input: '"
          << path_str << "'";
      return {};
    }
  }

  if (path.size() < 2) {
    LOG(ERROR)
        << "Insufficient number of ports (should be at least 2 but found "
        << path.size() << "). Input: '" << path_str << "'";
    return {};
  }

  return path;
}

// Parses tiled display properties from the TILE connector property
// |tile_blob|. TileProperty::scale_to_fit_display is not populated here as this
// information is not available in the TILE blob. Tile property blob is encoded
// as:
// "group_id:tile_is_single_monitor:num_h_tile:num_v_tile:tile_h_loc:tile_v_loc
//  :tile_h_size:tile_v_size"
// e.g. 313a313a323a313a303a303a323536303a3238383000 == 1:1:2:1:0:0:2560:2880
// tile_is_single_monitor is not used as all tiles in a single group are to be
// treated as a single monitor for simplicity.
std::optional<TileProperty> ParseTileBlob(
    const drmModePropertyBlobRes& tile_blob) {
  if (!tile_blob.length) {
    LOG(ERROR) << "TILE property blob is empty.";
    return std::nullopt;
  }

  const std::string tile_str(
      static_cast<char*>(tile_blob.data),
      base::strict_cast<std::string::size_type>(tile_blob.length));
  std::string_view tile_string_piece(tile_str);
  tile_string_piece = base::TrimString(tile_string_piece, std::string("\0", 1u),
                                       base::TRIM_TRAILING);

  std::vector<std::string_view> tile_properties = base::SplitStringPiece(
      tile_string_piece, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (tile_properties.size() != 8) {
    LOG(ERROR) << "Some of the values in the TILE property are missing. "
                  "Expected 8, got "
               << tile_properties.size() << ". TILE blob: " << tile_str;
    return std::nullopt;
  }

  TileProperty tile_property;
  int num_tiles_horiz, num_tiles_vert, tile_loc_horiz, tile_loc_vert,
      tile_size_horiz, tile_size_vert;
  std::vector<std::pair<size_t /*tile properties index*/, int*>>
      tile_properties_ptrs = {{0, &tile_property.group_id},
                              // Skip {1, tile_is_single_monitor}
                              {2, &num_tiles_horiz},
                              {3, &num_tiles_vert},
                              {4, &tile_loc_horiz},
                              {5, &tile_loc_vert},
                              {6, &tile_size_horiz},
                              {7, &tile_size_vert}};

  for (auto& [index, property_ptr] : tile_properties_ptrs) {
    if (!base::StringToInt(tile_properties[index], property_ptr)) {
      LOG(ERROR) << "Could not convert string \"" << tile_properties[index]
                 << "\" at index #" << index
                 << " of the TILE property to an int. TILE blob: " << tile_str;
      return std::nullopt;
    }
  }

  tile_property.tile_size.SetSize(tile_size_horiz, tile_size_vert);
  tile_property.tile_layout.SetSize(num_tiles_horiz, num_tiles_vert);
  tile_property.location.SetPoint(tile_loc_horiz, tile_loc_vert);

  return tile_property;
}

bool IsAddfb2ModifierCapable(const DrmWrapper& drm) {
  uint64_t addfb2_mod_cap = 0;
  return drm.GetCapability(DRM_CAP_ADDFB2_MODIFIERS, &addfb2_mod_cap) &&
         addfb2_mod_cap;
}

std::string GetEnumNameForProperty(
    const drmModePropertyRes& property,
    const drmModeObjectProperties& property_values) {
  for (uint32_t prop_idx = 0; prop_idx < property_values.count_props;
       ++prop_idx) {
    if (property_values.props[prop_idx] != property.prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property.count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property.enums[enum_idx];
      if (property_enum.value == property_values.prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::optional<std::string> GetDrmDriverNameFromFd(int fd) {
  ScopedDrmVersionPtr version(drmGetVersion(fd));
  if (!version) {
    LOG(ERROR) << "Failed to query DRM version";
    return std::nullopt;
  }

  return std::string(version->name, version->name_len);
}

std::optional<std::string> GetDrmDriverNameFromPath(
    const char* device_file_name) {
  base::ScopedFD fd(open(device_file_name, O_RDWR));
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open DRM device " << device_file_name;
    return std::nullopt;
  }

  return GetDrmDriverNameFromFd(fd.get());
}

std::vector<const char*> GetPreferredDrmDrivers() {
  const base::FilePath dmi_dir("/sys/class/dmi/id");

  const auto sys_vendor = ReadFileAndTrim(dmi_dir.Append("sys_vendor"));
  const auto product_name = ReadFileAndTrim(dmi_dir.Append("product_name"));

  // The iMac 12.1 and 12.2 have an integrated Intel GPU that isn't connected
  // to any real outputs. Prefer the Radeon card instead.
  if (sys_vendor == "Apple Inc." &&
      (product_name == "iMac12,1" || product_name == "iMac12,2")) {
    return {"radeon"};
  }

  // Default order.
  return {"i915", "amdgpu", "virtio_gpu"};
}

void ConsolidateTiledDisplayInfo(
    std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
        display_infos) {
  // Ignore all non-tiled displays, group all tile displays into |tile_groups|
  // by tile group IDs.
  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> new_display_infos;
  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
      nontiled_display_infos;
  std::unordered_map<
      int /*tile_group_id*/,
      std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>>
      tile_groups;
  for (auto& info : display_infos) {
    const std::optional<TileProperty>& tile_property = info->tile_property();
    if (tile_property.has_value()) {
      tile_groups[tile_property->group_id].push_back(std::move(info));
    } else {
      nontiled_display_infos.push_back(std::move(info));
    }
  }
  new_display_infos = std::move(nontiled_display_infos);

  // For each tile display group, determine the primary tile and drop others in
  // the group.
  for (auto& [_, tile_infos] : tile_groups) {
    const HardwareDisplayControllerInfo* primary_tile_info_ptr =
        GetPrimaryTileInfo(tile_infos);
    std::unique_ptr<HardwareDisplayControllerInfo> primary_tile_info =
        PopPrimaryTileInfo(primary_tile_info_ptr, tile_infos);

    for (auto& nonprimary_tile_info : tile_infos) {
      primary_tile_info->AcquireNonprimaryTileInfo(
          std::move(nonprimary_tile_info));
    }

    new_display_infos.push_back(std::move(primary_tile_info));
  }

  display_infos = std::move(new_display_infos);
}

gfx::Size GetTotalTileDisplaySize(const TileProperty& tile_property) {
  const gfx::Size& layout = tile_property.tile_layout;
  const gfx::Size& tile_size = tile_property.tile_size;
  return gfx::Size(tile_size.width() * layout.width(),
                   tile_size.height() * layout.height());
}

}  // namespace ui

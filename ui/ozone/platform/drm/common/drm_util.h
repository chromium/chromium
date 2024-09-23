// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace display {
class DisplayMode;
}  // namespace display

namespace ui {
class HardwareDisplayControllerInfo;

// TODO(b/193019614): clean |kMaxDrmCount|'s and |kMaxDrmConnectors|'s
// assignment up once EDID-based ID migration is complete and the flag is
// removed.
// It is safe to assume there will be no more than 256 connected DRM devices.
const size_t kMaxDrmCount =
    display::features::IsEdidBasedDisplayIdsEnabled() ? 256u : 16u;

// It is safe to assume there will be no more than 256 connectors per DRM.
const size_t kMaxDrmConnectors =
    display::features::IsEdidBasedDisplayIdsEnabled() ? 256u : 16u;

// Using a moderate size e.g. 256 for the cursor is enough in most cases.
const int kMaxCursorBufferSize = 256;

// DRM property names.
const char kContentProtectionKey[] = "Content Protection Key";
const char kContentProtection[] = "Content Protection";
const char kHdcpContentType[] = "HDCP Content Type";

const char kColorSpace[] = "Colorspace";
const char kColorSpaceBT2020RGBEnumName[] = "BT2020_RGB";
const char kColorSpaceDefaultEnumName[] = "Default";

const char kHdrOutputMetadata[] = "HDR_OUTPUT_METADATA";

constexpr char kPrivacyScreenPropertyNameLegacy[] = "privacy-screen";
constexpr char kPrivacyScreenHwStatePropertyName[] = "privacy-screen hw-state";
constexpr char kPrivacyScreenSwStatePropertyName[] = "privacy-screen sw-state";

constexpr char kVrrCapablePropertyName[] = "vrr_capable";
constexpr char kVrrEnabledPropertyName[] = "VRR_ENABLED";

// DRM property enum to internal type mappings.
template <typename InternalType>
struct DrmPropertyEnumToInternalTypeMapping {
  const char* drm_enum;
  const InternalType internal_state;
};

constexpr std::array<
    DrmPropertyEnumToInternalTypeMapping<display::ContentProtectionMethod>,
    2>
    kHdcpContentTypeStates{
        {{"HDCP Type0", display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0},
         {"HDCP Type1", display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1}}};

constexpr std::array<DrmPropertyEnumToInternalTypeMapping<display::HDCPState>,
                     3>
    kContentProtectionStates{{{"Undesired", display::HDCP_STATE_UNDESIRED},
                              {"Desired", display::HDCP_STATE_DESIRED},
                              {"Enabled", display::HDCP_STATE_ENABLED}}};

constexpr std::
    array<DrmPropertyEnumToInternalTypeMapping<display::PrivacyScreenState>, 4>
        kPrivacyScreenStates{{{"Disabled", display::kDisabled},
                              {"Enabled", display::kEnabled},
                              {"Disabled-locked", display::kDisabledLocked},
                              {"Enabled-locked", display::kEnabledLocked}}};

// Looks-up and parses the native display configurations returning all available
// displays and CRTCs that weren't picked as best CRTC for each connector.
// TODO(markyacoub): Create unit tests that tests the different bits and pieces
// that this function goes through.
std::pair<std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>,
          std::vector<uint32_t>>
GetDisplayInfosAndInvalidCrtcs(const DrmWrapper& drm);

// Returns the display infos parsed in |GetDisplayInfosAndInvalidCrtcs|
std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
GetAvailableDisplayControllerInfos(const DrmWrapper& drm);

// Returns a bitmask of possible CRTCs for at least one encoder in
// |encoder_ids|. The index in the bitmask corresponds to drm_crtc_index().
uint32_t GetPossibleCrtcsBitmaskFromEncoders(
    const DrmWrapper& drm,
    const std::vector<uint32_t>& encoder_ids);

// Returns a list of all possible CRTCs for encoders with IDs in |encoder_ids|.
std::vector<uint32_t> GetPossibleCrtcIdsFromBitmask(
    const DrmWrapper& drm,
    const uint32_t possible_crtcs_bitmask);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

std::unique_ptr<display::DisplayMode> CreateDisplayMode(
    const drmModeModeInfo& mode,
    const std::optional<uint16_t>& vsync_rate_min_from_edid);

// Returns a virtual mode based on |base_mode| with its vtotal altered to
// achieve the specified |virtual_refresh_rate|, or nullptr if it could not be
// created.
std::unique_ptr<drmModeModeInfo> CreateVirtualMode(
    const drmModeModeInfo& base_mode,
    float virtual_refresh_rate);

// Extracts the display modes list from |info| as well as the current and native
// display modes given the |active_pixel_size| which is retrieved from the first
// detailed timing descriptor in the EDID.
display::DisplaySnapshot::DisplayModeList ExtractDisplayModes(
    HardwareDisplayControllerInfo* info,
    const gfx::Size& active_pixel_size,
    const display::DisplayMode** out_current_mode,
    const display::DisplayMode** out_native_mode);

// |info| provides the DRM information related to the display, |drm| is the
// access point to the DRM device to which |info| is related to.
std::unique_ptr<display::DisplaySnapshot> CreateDisplaySnapshot(
    const DrmWrapper& drm,
    HardwareDisplayControllerInfo* info,
    uint8_t device_index);

int GetFourCCFormatForOpaqueFramebuffer(gfx::BufferFormat format);

gfx::Size GetMaximumCursorSize(const DrmWrapper& drm);

ScopedDrmPropertyPtr FindDrmProperty(const DrmWrapper& drm,
                                     drmModeObjectProperties* properties,
                                     const char* name);

bool HasColorCorrectionMatrix(const DrmWrapper& drm, drmModeCrtc* crtc);

bool MatchMode(const display::DisplayMode& display_mode,
               const drmModeModeInfo& m);

const gfx::Size ModeSize(const drmModeModeInfo& mode);

float ModeRefreshRate(const drmModeModeInfo& mode);

bool ModeIsInterlaced(const drmModeModeInfo& mode);

// Computes the precise minimum vsync rate using the mode's timing details.
// The value obtained from the EDID has a loss of precision due to being an
// integer. The precise rate must correspond to an integer valued vtotal.
const std::optional<float> ModeVSyncRateMin(
    const drmModeModeInfo& mode,
    const std::optional<uint16_t>& vsync_rate_min_from_edid);

bool IsVrrCapable(const DrmWrapper& drm, drmModeConnector* connector);

bool IsVrrEnabled(const DrmWrapper& drm, drmModeCrtc* crtc);

display::VariableRefreshRateState GetVariableRefreshRateState(
    const DrmWrapper& drm,
    HardwareDisplayControllerInfo* info);

const char* GetNameForColorspace(const gfx::ColorSpace color_space);
uint64_t GetEnumValueForName(const DrmWrapper& drm,
                             int property_id,
                             const char* str);

// Checks if |mode_size| corresponds to a tile mode size according to
// |tile_property|. Note that this method does not return true for
// tile-composited mode.
bool IsTileMode(const gfx::Size mode_size, const TileProperty& tile_property);

const gfx::Point GetTileCrtcOffset(const TileProperty& tiled_property);

std::vector<uint64_t> ParsePathBlob(const drmModePropertyBlobRes& path_blob);

std::optional<TileProperty> ParseTileBlob(
    const drmModePropertyBlobRes& tile_blob);

// Whether or not |drm| supports supplying modifiers for AddFramebuffer2.
bool IsAddfb2ModifierCapable(const DrmWrapper& drm);

// Extracts the DRM |property| current value's enum. Returns an empty string
// upon failure.
std::string GetEnumNameForProperty(
    const drmModePropertyRes& property,
    const drmModeObjectProperties& property_values);

// Extracts the DRM property's numeric value that maps to |internal_state|.
// Returns the maximal numeric value for uint64_t upon failure.
template <typename InternalType, typename DrmPropertyToInternalTypeMap>
uint64_t GetDrmValueForInternalType(const InternalType& internal_state,
                                    const drmModePropertyRes& property,
                                    const DrmPropertyToInternalTypeMap& map) {
  std::string drm_enum;
  for (const auto& pair : map) {
    if (pair.internal_state == internal_state) {
      drm_enum = pair.drm_enum;
      break;
    }
  }
  DCHECK(!drm_enum.empty())
      << "Property " << property.name
      << " has no enum value for the requested internal state (value <"
      << internal_state << ">).";

  for (int i = 0; i < property.count_enums; ++i) {
    if (drm_enum == property.enums[i].name)
      return property.enums[i].value;
  }

  NOTREACHED_IN_MIGRATION()
      << "Failed to extract DRM value for property '" << property.name
      << "' and enum '" << drm_enum << "'";
  return std::numeric_limits<uint64_t>::max();
}

// Returns the internal type value that maps to the DRM property's current
// value. Returns nullptr upon failure.
template <typename InternalType, size_t size>
const InternalType* GetDrmPropertyCurrentValueAsInternalType(
    const std::array<DrmPropertyEnumToInternalTypeMapping<InternalType>, size>&
        array,
    const drmModePropertyRes& property,
    const drmModeObjectProperties& property_values) {
  const std::string drm_enum =
      GetEnumNameForProperty(property, property_values);
  if (drm_enum.empty()) {
    LOG(ERROR) << "Failed to fetch DRM enum for property '" << property.name
               << "'";
    return nullptr;
  }

  for (const auto& pair : array) {
    if (drm_enum == pair.drm_enum) {
      VLOG(3) << "Internal state value: " << pair.internal_state << " ("
              << drm_enum << ")";
      return &pair.internal_state;
    }
  }

  NOTREACHED_IN_MIGRATION()
      << "Failed to extract internal value for DRM property '" << property.name
      << "'";
  return nullptr;
}

// Returns the internal type value that maps to |drm_enum| within |array|.
// Returns nullptr upon failure.
template <typename InternalType, size_t size>
const InternalType* GetInternalTypeValueFromDrmEnum(
    const std::string& drm_enum,
    const std::array<DrmPropertyEnumToInternalTypeMapping<InternalType>, size>&
        array) {
  if (drm_enum.empty()) {
    LOG(ERROR) << "DRM property value enum is empty.";
    return nullptr;
  }

  for (const auto& pair : array) {
    if (drm_enum == pair.drm_enum) {
      VLOG(3) << "Internal state value: " << pair.internal_state << " ("
              << drm_enum << ")";
      return &pair.internal_state;
    }
  }

  LOG(ERROR) << "Failed to extract internal value for DRM property enum '"
             << drm_enum << "'";
  return nullptr;
}

// Get the DRM driver name.
std::optional<std::string> GetDrmDriverNameFromFd(int fd);
std::optional<std::string> GetDrmDriverNameFromPath(
    const char* device_file_name);

// Get an ordered list of preferred DRM driver names for the
// system. Uses DMI information to determine what the system is.
std::vector<const char*> GetPreferredDrmDrivers();

// Given |display_infos|, where each HardwareDisplayControllerInfo can represent
// a regular display or a tile, consolidate all tiles belonging to the same
// display into one HardwareDisplayControllerInfo. All non-tile
// HardwareDisplayControllerInfo will not be altered.
void ConsolidateTiledDisplayInfo(
    std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>& display_infos);

// Get the total tile-composited size of a tiled display.
gfx::Size GetTotalTileDisplaySize(const TileProperty& tile_property);

// A custom comparator of gfx::Size used to sort cursor sizes.
struct CursorSizeComparator {
  bool operator()(const gfx::Size& a, const gfx::Size& b) const {
    if (a.GetArea() == b.GetArea()) {
      if (a.width() == b.width()) {
        return a.height() < b.height();
      } else {
        return a.width() < b.width();
      }
    } else {
      return a.GetArea() < b.GetArea();
    }
  }
};
}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

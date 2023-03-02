// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace display {
class DisplayMode;
}  // namespace display

namespace gfx {
class Point;
}

namespace ui {

// It is safe to assume there will be no more than 256 connected DRM devices.
constexpr int kMaxDrmCount = 256u;

// It is safe to assume there will be no more than 256 connectors per DRM.
constexpr int kMaxDrmConnectors = 256u;

// DRM property names.
const char kContentProtectionKey[] = "Content Protection Key";
const char kContentProtection[] = "Content Protection";
const char kHdcpContentType[] = "HDCP Content Type";

constexpr char kPrivacyScreenPropertyNameLegacy[] = "privacy-screen";
constexpr char kPrivacyScreenHwStatePropertyName[] = "privacy-screen hw-state";
constexpr char kPrivacyScreenSwStatePropertyName[] = "privacy-screen sw-state";

constexpr char kVrrCapablePropertyName[] = "vrr_capable";
constexpr char kVrrEnabledPropertyName[] = "VRR_ENABLED";

// DRM property enum to internal type mappings.
template <typename InternalType>
struct DrmPropertyEnumToInternalTypeMapping {
  const char* drm_enum;
  const InternalType& internal_state;
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

// Representation of the information required to initialize and configure a
// native display. |index| is the position of the connection and will be
// used to generate a unique identifier for the display.
class HardwareDisplayControllerInfo {
 public:
  HardwareDisplayControllerInfo(ScopedDrmConnectorPtr connector,
                                ScopedDrmCrtcPtr crtc,
                                uint8_t index);

  HardwareDisplayControllerInfo(const HardwareDisplayControllerInfo&) = delete;
  HardwareDisplayControllerInfo& operator=(
      const HardwareDisplayControllerInfo&) = delete;

  ~HardwareDisplayControllerInfo();

  drmModeConnector* connector() const { return connector_.get(); }
  drmModeCrtc* crtc() const { return crtc_.get(); }
  uint8_t index() const { return index_; }

  ScopedDrmConnectorPtr ReleaseConnector() { return std::move(connector_); }

 private:
  ScopedDrmConnectorPtr connector_;
  ScopedDrmCrtcPtr crtc_;
  uint8_t index_;
};

using HardwareDisplayControllerInfoList =
    std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>;

// Looks-up and parses the native display configurations returning all available
// displays and CRTCs that weren't picked as best CRTC for each connector.
// TODO(markyacoub): Create unit tests that tests the different bits and pieces
// that this function goes through.
std::pair<HardwareDisplayControllerInfoList, std::vector<uint32_t>>
GetDisplayInfosAndInvalidCrtcs(const DrmWrapper& drm);

// Returns the display infos parsed in |GetDisplayInfosAndInvalidCrtcs|
HardwareDisplayControllerInfoList GetAvailableDisplayControllerInfos(
    const DrmWrapper& drm);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

std::unique_ptr<display::DisplayMode> CreateDisplayMode(
    const drmModeModeInfo& mode);

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
    uint8_t device_index,
    const gfx::Point& origin,
    const display::DrmFormatsAndModifiers& drm_formats_and_modifiers);

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

bool IsVrrCapable(const DrmWrapper& drm, drmModeConnector* connector);

bool IsVrrEnabled(const DrmWrapper& drm, drmModeCrtc* crtc);

display::VariableRefreshRateState GetVariableRefreshRateState(
    const DrmWrapper& drm,
    HardwareDisplayControllerInfo* info);

uint64_t GetEnumValueForName(const DrmWrapper& drm,
                             int property_id,
                             const char* str);

std::vector<uint64_t> ParsePathBlob(const drmModePropertyBlobRes& path_blob);

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

  NOTREACHED() << "Failed to extract DRM value for property '" << property.name
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

  NOTREACHED() << "Failed to extract internal value for DRM property '"
               << property.name << "'";
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
absl::optional<std::string> GetDrmDriverNameFromFd(int fd);
absl::optional<std::string> GetDrmDriverNameFromPath(
    const char* device_file_name);

// Get an ordered list of preferred DRM driver names for the
// system. Uses DMI information to determine what the system is.
std::vector<const char*> GetPreferredDrmDrivers();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

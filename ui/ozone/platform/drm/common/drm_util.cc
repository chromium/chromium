// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

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

display::DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
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

bool HasPerPlaneColorCorrectionMatrix(const DrmWrapper& drm,
                                      drmModeCrtc* crtc) {
  ScopedDrmPlaneResPtr plane_resources = drm.GetPlaneResources();
  DCHECK(plane_resources);
  for (uint32_t i = 0; i < plane_resources->count_planes; ++i) {
    ScopedDrmObjectPropertyPtr plane_props = drm.GetObjectProperties(
        plane_resources->planes[i], DRM_MODE_OBJECT_PLANE);
    DCHECK(plane_props);

    if (!FindDrmProperty(drm, plane_props.get(), "PLANE_CTM")) {
      return false;
    }
  }

  // On legacy, if no planes are exposed then the property isn't available.
  return plane_resources->count_planes > 0;
}

// Read a file and trim whitespace. If the file can't be read, returns
// nullopt.
absl::optional<std::string> ReadFileAndTrim(const base::FilePath& path) {
  std::string data;
  if (!base::ReadFileToString(path, &data))
    return absl::nullopt;

  return std::string(
      base::TrimWhitespaceASCII(data, base::TrimPositions::TRIM_ALL));
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
    return display::kVrrNotCapable;
  }

  if (IsVrrEnabled(drm, info->crtc())) {
    return display::kVrrEnabled;
  }

  return display::kVrrDisabled;
}

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc,
    uint8_t index)
    : connector_(std::move(connector)), crtc_(std::move(crtc)), index_(index) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() = default;

std::pair<HardwareDisplayControllerInfoList, std::vector<uint32_t>>
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
    if (!connector)
      continue;

    if (connector->connection == DRM_MODE_CONNECTED &&
        connector->count_modes != 0) {
      available_connectors.push_back(connector.get());
    }

    connectors.emplace_back(std::move(connector));
  }

  base::flat_map<drmModeConnector*, int> connector_crtcs;
  for (auto* c : available_connectors) {
    uint32_t possible_crtcs = 0;
    for (int i = 0; i < c->count_encoders; ++i) {
      ScopedDrmEncoderPtr encoder = drm.GetEncoder(c->encoders[i]);
      if (!encoder)
        continue;
      possible_crtcs |= encoder->possible_crtcs;
    }
    connector_crtcs[c] = possible_crtcs;
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
    auto iter = base::ranges::find(connectors, c, &ScopedDrmConnectorPtr::get);
    DCHECK(iter != connectors.end());
    // |connectors.size()| <= 256, so |index| should be between 0-255.
    const uint8_t index = iter - connectors.begin();
    DCHECK_LT(index, connectors.size());
    displays.push_back(std::make_unique<HardwareDisplayControllerInfo>(
        std::move(*iter), std::move(crtc), index));
  }

  return std::make_pair(std::move(displays), std::move(invalid_crtcs));
}

HardwareDisplayControllerInfoList GetAvailableDisplayControllerInfos(
    const DrmWrapper& drm) {
  return GetDisplayInfosAndInvalidCrtcs(drm).first;
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
    const drmModeModeInfo& mode) {
  return std::make_unique<display::DisplayMode>(
      gfx::Size(mode.hdisplay, mode.vdisplay),
      mode.flags & DRM_MODE_FLAG_INTERLACE, GetRefreshRate(mode));
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
    modes.push_back(CreateDisplayMode(mode));

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
  const display::DisplayConnectionType type = GetDisplayType(info->connector());
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
  const bool has_color_correction_matrix =
      HasColorCorrectionMatrix(drm, info->crtc()) ||
      HasPerPlaneColorCorrectionMatrix(drm, info->crtc());
  // On rk3399 we can set a color correction matrix that will be applied in
  // linear space. https://crbug.com/839020 to track if it will be possible to
  // disable the per-plane degamma/gamma.
  const bool color_correction_in_linear_space =
      has_color_correction_matrix && drm.GetDriverName() == "rockchip";
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
  gfx::ColorSpace display_color_space;
  uint32_t bits_per_channel = 8u;
  absl::optional<gfx::HDRStaticMetadata> hdr_static_metadata{};
  // Active pixels size from the first detailed timing descriptor in the EDID.
  gfx::Size active_pixel_size;
  absl::optional<gfx::Range> vertical_display_range_limits;

  ScopedDrmPropertyBlobPtr edid_blob(
      GetDrmPropertyBlob(drm, info->connector(), "EDID"));
  base::UmaHistogramBoolean("DrmUtil.CreateDisplaySnapshot.HasEdidBlob",
                            !!edid_blob);
  std::vector<uint8_t> edid;
  if (edid_blob) {
    DCHECK(edid_blob->length);
    edid.assign(static_cast<uint8_t*>(edid_blob->data),
                static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);
    const bool is_external = type != display::DISPLAY_CONNECTION_TYPE_INTERNAL;
    display::EdidParser edid_parser(edid, is_external);
    display_name = edid_parser.display_name();
    active_pixel_size = edid_parser.active_pixel_size();
    product_code = edid_parser.GetProductCode();
    port_display_id = edid_parser.GetIndexBasedDisplayId(display_index);
    edid_display_id = edid_parser.GetEdidBasedDisplayId();
    year_of_manufacture = edid_parser.year_of_manufacture();
    has_overscan =
        edid_parser.has_overscan_flag() && edid_parser.overscan_flag();
    display_color_space = display::GetColorSpaceFromEdid(edid_parser);
    base::UmaHistogramBoolean("DrmUtil.CreateDisplaySnapshot.IsHDR",
                              display_color_space.IsHDR());
    bits_per_channel = std::max(edid_parser.bits_per_channel(), 0);
    base::UmaHistogramCounts100("DrmUtil.CreateDisplaySnapshot.BitsPerChannel",
                                bits_per_channel);
    hdr_static_metadata = edid_parser.hdr_static_metadata();
    vertical_display_range_limits =
        variable_refresh_rate_state == display::kVrrNotCapable
            ? absl::nullopt
            : edid_parser.vertical_display_range_limits();
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << info->connector()->connector_id;
  }

  const display::DisplayMode* current_mode = nullptr;
  const display::DisplayMode* native_mode = nullptr;
  display::DisplaySnapshot::DisplayModeList modes =
      ExtractDisplayModes(info, active_pixel_size, &current_mode, &native_mode);

  const display::DrmFormatsAndModifiers drm_formats_and_modifiers =
      drm.GetFormatsAndModifiersForCrtc(info->crtc()->crtc_id);

  return std::make_unique<display::DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      gfx::Point(), physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, has_color_correction_matrix,
      color_correction_in_linear_space, display_color_space, bits_per_channel,
      hdr_static_metadata, display_name, drm.device_path(), std::move(modes),
      panel_orientation, edid, current_mode, native_mode, product_code,
      year_of_manufacture, maximum_cursor_size, variable_refresh_rate_state,
      vertical_display_range_limits, drm_formats_and_modifiers);
}

int GetFourCCFormatForOpaqueFramebuffer(gfx::BufferFormat format) {
  // DRM atomic interface doesn't currently support specifying an alpha
  // blending. We can simulate disabling alpha bleding creating an fb
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
      NOTREACHED();
      return 0;
  }
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
  NOTREACHED();
  return 0;
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
  base::StringPiece path_string_piece(path_str);
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

  NOTREACHED();
  return std::string();
}

absl::optional<std::string> GetDrmDriverNameFromFd(int fd) {
  ScopedDrmVersionPtr version(drmGetVersion(fd));
  if (!version) {
    LOG(ERROR) << "Failed to query DRM version";
    return absl::nullopt;
  }

  return std::string(version->name, version->name_len);
}

absl::optional<std::string> GetDrmDriverNameFromPath(
    const char* device_file_name) {
  base::ScopedFD fd(open(device_file_name, O_RDWR));
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open DRM device " << device_file_name;
    return absl::nullopt;
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
      (product_name == "iMac12,1" || product_name == "iMac12,2"))
    return {"radeon"};

  // Default order.
  return {"i915", "amdgpu", "virtio_gpu"};
}

}  // namespace ui

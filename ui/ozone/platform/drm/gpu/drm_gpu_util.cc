// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

namespace {

struct PossibleCrtcsForConnector {
  uint32_t connector_id;
  std::vector<uint32_t> possible_crtcs;
};

// Recursively build out all possible permutations of CRTC-connector pairings
// given a set of connectors and their possible CRTCs. Each CRTC/connector can
// only be used once per permutation (CrtcConnectorPairs).
// |connectors_it| is an iterator of |connectors| that tracks which connector
// has been used (connector left of |connectors_it|). Passing around
// |connectors_it| is safe due to the constness of |connectors|.
// |crtcs_used_in_current_permutation| tracks if a CRTC has already been used as
// part of the current permutation.
// For example:
//   Connector A can have CRTCs 1, 2, 3
//   Connector B can have CRTCs 2, 3
//   Connector C can have CRTCs 1, 3
// Returned pairings would be:
//   {{A, 1}, {B, 2}, {C, 3}},
//   {{A, 2}, {B, 1}, {C, 3}},
//   {{A, 3}, {B, 2}, {C, 1}}
// But not {{A, 1}, {B, 3}, {C, nothing}} as connector C must also be assigned
// to a valid CRTC and permutations like this are discarded.
std::vector<CrtcConnectorPairs> BuildCrtcConnectorPermutations(
    const std::vector<PossibleCrtcsForConnector>& connectors,
    std::vector<PossibleCrtcsForConnector>::iterator connectors_it,
    base::flat_set<uint32_t /*crtc_id*/>& crtcs_used_in_current_permutation) {
  if (connectors_it == connectors.end()) {
    return {};
  }

  std::vector<CrtcConnectorPairs> permutations;
  const PossibleCrtcsForConnector& connector = *connectors_it;
  // Terminate the recursion once |connectors_it| reaches the end of
  // |connectors|. Also ensures that all |permutations| will have all the
  // connectors paired up with a CRTC.
  if (connectors_it == connectors.end() - 1) {
    // Possible permutations at this point are all unused CRTCs + the current
    // connector.
    for (const uint32_t crtc_id : connector.possible_crtcs) {
      if (!crtcs_used_in_current_permutation.contains(crtc_id)) {
        permutations.push_back({CrtcConnectorPair{
            .crtc_id = crtc_id, .connector_id = connector.connector_id}});
      }
    }
    return permutations;
  }

  for (const uint32_t crtc_id : connector.possible_crtcs) {
    // Skip |crtc_id| if it is already being used in this permutation.
    if (crtcs_used_in_current_permutation.contains(crtc_id)) {
      continue;
    }

    // Mark |crtc| as being in use for the current permutation so that it isn't
    // used multiple times per CrtcConnectorPairs.
    crtcs_used_in_current_permutation.insert(crtc_id);
    std::vector<CrtcConnectorPairs> next_connector_permutations =
        BuildCrtcConnectorPermutations(connectors, connectors_it + 1,
                                       crtcs_used_in_current_permutation);
    crtcs_used_in_current_permutation.erase(crtc_id);

    // Add the current |crtc|-|connector| pair to |next_connector_permutations|
    // as part of recursively building up CrtcConnectorPairs.
    for (auto& permutation : next_connector_permutations) {
      permutation.push_back(CrtcConnectorPair{
          .crtc_id = crtc_id, .connector_id = connector.connector_id});
    }
    permutations.insert(permutations.end(), next_connector_permutations.begin(),
                        next_connector_permutations.end());
  }

  return permutations;
}

// Constants for parsing CTM values.
constexpr uint64_t kCtmSignMask = (1ull << 63);
constexpr uint64_t kCtmValueMask = ~(1ull << 63);
constexpr float kCtmValueScale = static_cast<float>(1ull << 32);

}  // namespace

ControllerConfigParams::ControllerConfigParams(
    int64_t display_id,
    scoped_refptr<DrmDevice> drm,
    uint32_t crtc,
    uint32_t connector,
    gfx::Point origin,
    std::unique_ptr<drmModeModeInfo> pmode,
    bool enable_vrr,
    uint64_t base_connector)
    : display_id(display_id),
      drm(drm),
      crtc(crtc),
      connector(connector),
      base_connector_id(base_connector ? base_connector
                                       : static_cast<uint64_t>(connector)),
      origin(origin),
      mode(std::move(pmode)),
      enable_vrr(enable_vrr) {}

ControllerConfigParams::ControllerConfigParams(
    const ControllerConfigParams& other)
    : display_id(other.display_id),
      drm(other.drm),
      crtc(other.crtc),
      connector(other.connector),
      base_connector_id(other.base_connector_id),
      origin(other.origin),
      enable_vrr(other.enable_vrr) {
  if (other.mode) {
    drmModeModeInfo mode_obj = *other.mode.get();
    mode = std::make_unique<drmModeModeInfo>(mode_obj);
  }
}

ControllerConfigParams::ControllerConfigParams(ControllerConfigParams&& other)
    : display_id(other.display_id),
      drm(other.drm),
      crtc(other.crtc),
      connector(other.connector),
      base_connector_id(other.base_connector_id),
      origin(other.origin),
      enable_vrr(other.enable_vrr) {
  if (other.mode) {
    drmModeModeInfo mode_obj = *other.mode.get();
    mode = std::make_unique<drmModeModeInfo>(mode_obj);
  }
}

ControllerConfigParams::~ControllerConfigParams() = default;

bool GetDrmPropertyForName(DrmWrapper* drm,
                           drmModeObjectProperties* properties,
                           const std::string& name,
                           DrmWrapper::Property* property) {
  for (uint32_t i = 0; i < properties->count_props; ++i) {
    ScopedDrmPropertyPtr drm_property(drm->GetProperty(properties->props[i]));
    if (name != drm_property->name)
      continue;

    property->id = drm_property->prop_id;
    property->value = properties->prop_values[i];
    if (property->id)
      return true;
  }

  return false;
}

bool AddPropertyIfValid(drmModeAtomicReq* property_set,
                        uint32_t object_id,
                        const DrmWrapper::Property& property) {
  if (!property.id)
    return true;

  int ret = drmModeAtomicAddProperty(property_set, object_id, property.id,
                                     property.value);
  if (ret < 0) {
    LOG(ERROR) << "Failed to set property object_id=" << object_id
               << " property_id=" << property.id
               << " property_value=" << property.value << " error=" << -ret;
    return false;
  }

  return true;
}

ScopedDrmColorLutPtr CreateLutBlob(const display::GammaCurve& source,
                                   size_t size) {
  TRACE_EVENT0("drm", "CreateLutBlob");
  if (source.IsDefaultIdentity()) {
    return nullptr;
  }

  ScopedDrmColorLutPtr lut(
      static_cast<drm_color_lut*>(malloc(sizeof(drm_color_lut) * size)));
  drm_color_lut* p = lut.get();
  for (size_t i = 0; i < size; ++i) {
    // Be robust to `size` being 1, since some tests do this.
    source.Evaluate(i / std::max(size - 1.f, 1.f), p[i].red, p[i].green,
                    p[i].blue);
  }
  return lut;
}

bool ParseLutBlob(const void* data, size_t size, display::GammaCurve& result) {
  // LUT blobs are an array of drm_color_lut entries, and so the size of the
  // blob must be a multiple of the size of drm_color_lut.
  if (size % sizeof(drm_color_lut) != 0) {
    LOG(ERROR) << "Invalid size for LUT blob.";
    return false;
  }
  size_t entry_count = size / sizeof(drm_color_lut);
  const drm_color_lut* entries = reinterpret_cast<const drm_color_lut*>(data);
  std::vector<display::GammaRampRGBEntry> lut(entry_count);
  for (size_t i = 0; i < entry_count; ++i) {
    lut[i].r = entries[i].red;
    lut[i].g = entries[i].green;
    lut[i].b = entries[i].blue;
  }
  result = display::GammaCurve(std::move(lut));
  return true;
}

ScopedDrmColorCtmPtr CreateCTMBlob(const skcms_Matrix3x3& color_matrix,
                                   bool negative_values_broken) {
  ScopedDrmColorCtmPtr ctm(
      static_cast<drm_color_ctm*>(drmMalloc(sizeof(drm_color_ctm))));
  for (size_t i = 0; i < 9; ++i) {
    float value = color_matrix.vals[i / 3][i % 3];
    if (value < 0) {
      if (negative_values_broken) {
        ctm->matrix[i] = 0;
      } else {
        ctm->matrix[i] =
            static_cast<uint64_t>(-value * kCtmValueScale) & kCtmValueMask;
        ctm->matrix[i] |= kCtmSignMask;
      }
    } else {
      ctm->matrix[i] =
          static_cast<uint64_t>(value * kCtmValueScale) & kCtmValueMask;
    }
  }

  return ctm;
}

bool ParseCTMBlob(const void* data, size_t size, skcms_Matrix3x3& result) {
  // CTM blobs must contain exactly 9 (3x3) numbers which are encoded in
  // uint64_ts.
  if (size != 9 * sizeof(uint64_t)) {
    LOG(ERROR) << "Invalid size for CTM blob.";
    return false;
  }
  const uint64_t* data_u64 = reinterpret_cast<const uint64_t*>(data);
  for (size_t i = 0; i < 9; ++i) {
    float sign = (data_u64[i] & kCtmSignMask) ? -1.f : 1.f;
    float value = (data_u64[i] & kCtmValueMask) / kCtmValueScale;
    result.vals[i / 3][i % 3] = sign * value;
  }
  return true;
}

ScopedDrmModeRectPtr CreateDCBlob(const gfx::Rect& rect) {
  // Damage rect can be empty, but sending empty or negative rects can result in
  // artifacting and black screens. Filter them out here.
  if (rect.width() <= 0 || rect.height() <= 0 || rect.x() < 0 || rect.y() < 0) {
    return nullptr;
  }

  ScopedDrmModeRectPtr dmg_rect(
      static_cast<drm_mode_rect*>(malloc(sizeof(drm_mode_rect))));
  dmg_rect->x1 = rect.x();
  dmg_rect->y1 = rect.y();
  dmg_rect->x2 = rect.right();
  dmg_rect->y2 = rect.bottom();
  return dmg_rect;
}

std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
GetDisplayInfosAndUpdateCrtcs(DrmWrapper& drm) {
  auto [displays, invalid_crtcs] = GetDisplayInfosAndInvalidCrtcs(drm);
  // Disable invalid CRTCs to allow the preferred CRTCs to be enabled later
  // instead.
  for (uint32_t crtc : invalid_crtcs) {
    drm.DisableCrtc(crtc);
    VLOG(1) << "Disabled undesired CRTC " << crtc;
  }
  return std::move(displays);
}

void DrmWriteIntoTraceHelper(const drmModeModeInfo& mode_info,
                             perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("name", mode_info.name);
  dict.Add("type", mode_info.type);
  dict.Add("flags", mode_info.flags);
  dict.Add("clock", mode_info.clock);
  dict.Add("hdisplay", mode_info.hdisplay);
  dict.Add("vdisplay", mode_info.vdisplay);
}

std::vector<CrtcConnectorPairs> GetAllCrtcConnectorPermutations(
    const DrmDevice& drm,
    const std::vector<ControllerConfigParams>& controllers_params) {
  if (controllers_params.empty()) {
    LOG(DFATAL) << "No connectors specified in controllers_params to generate "
                   "CRTC-connector pairings";
    return {};
  }

  std::vector<PossibleCrtcsForConnector> possible_crtcs_for_connectors;
  for (auto params : controllers_params) {
    const uint32_t possible_crtcs_bitmask =
        drm.plane_manager()->GetPossibleCrtcsBitmaskForConnector(
            params.connector);
    std::vector<uint32_t> possible_crtc_ids =
        GetPossibleCrtcIdsFromBitmask(drm, possible_crtcs_bitmask);
    possible_crtcs_for_connectors.push_back(
        {.connector_id = params.connector,
         .possible_crtcs = std::move(possible_crtc_ids)});
  }

  base::flat_set<uint32_t /*crtc_id*/> crtcs_used_in_current_permutation;
  std::vector<CrtcConnectorPairs> permutations = BuildCrtcConnectorPermutations(
      possible_crtcs_for_connectors, possible_crtcs_for_connectors.begin(),
      crtcs_used_in_current_permutation);

  return permutations;
}

void ApplyCrtcColorSpaceConversion(DrmWrapper* drm,
                                   uint32_t crtc_id,
                                   float rgb[3]) {
  // Look up all properties on this CRTC and create a helper lambda to look up
  // their blobs.
  ScopedDrmObjectPropertyPtr props(
      drm->GetObjectProperties(crtc_id, DRM_MODE_OBJECT_CRTC));
  if (!props) {
    return;
  }
  auto get_blob_by_name = [&](const char* name) {
    DrmDevice::Property property;
    if (!GetDrmPropertyForName(drm, props.get(), name, &property)) {
      return ScopedDrmPropertyBlobPtr(nullptr);
    }
    return drm->GetPropertyBlob(property.value);
  };

  // Apply DEGAMMA.
  ScopedDrmPropertyBlobPtr degamma_blob = get_blob_by_name("DEGAMMA_LUT");
  if (degamma_blob) {
    display::GammaCurve curve;
    if (ParseLutBlob(degamma_blob->data, degamma_blob->length, curve)) {
      curve.Evaluate(rgb);
    }
  }

  // Apply CTM.
  ScopedDrmPropertyBlobPtr ctm_blob = get_blob_by_name("CTM");
  if (ctm_blob) {
    skcms_Matrix3x3 ctm;
    if (ParseCTMBlob(ctm_blob->data, ctm_blob->length, ctm)) {
      float temp[3] = {0, 0, 0};
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          temp[i] += ctm.vals[i][j] * rgb[j];
        }
      }
      for (int i = 0; i < 3; ++i) {
        rgb[i] = temp[i];
      }
    }
  }

  // Apply GAMMA.
  ScopedDrmPropertyBlobPtr gamma_blob = get_blob_by_name("GAMMA_LUT");
  if (gamma_blob) {
    display::GammaCurve curve;
    if (ParseLutBlob(gamma_blob->data, gamma_blob->length, curve)) {
      curve.Evaluate(rgb);
    }
  }
}

}  // namespace ui

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

#include "base/containers/flat_map.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

typedef struct _drmModeModeInfo drmModeModeInfo;
struct skcms_Matrix3x3;

namespace ui {

struct ControllerConfigParams {
  ControllerConfigParams(int64_t display_id,
                         scoped_refptr<DrmDevice> drm,
                         uint32_t crtc,
                         uint32_t connector,
                         gfx::Point origin,
                         std::unique_ptr<drmModeModeInfo> pmode,
                         bool enable_vrr = false,
                         uint64_t base_connector_id = 0);
  ControllerConfigParams(const ControllerConfigParams& other);
  ControllerConfigParams(ControllerConfigParams&& other);
  ~ControllerConfigParams();

  const int64_t display_id;
  const scoped_refptr<DrmDevice> drm;
  uint32_t crtc;
  const uint32_t connector;
  const uint64_t base_connector_id;
  const gfx::Point origin;
  std::unique_ptr<drmModeModeInfo> mode;
  const bool enable_vrr;
};

using ConnectorCrtcMap =
    base::flat_map<uint32_t /*connector_id*/, uint32_t /*crtc_id*/>;

// Describes assignment of CRTC with |crtc_id| to a connector with
// |connector_id|.
struct CrtcConnectorPair {
  uint32_t crtc_id;
  uint32_t connector_id;
};
using CrtcConnectorPairs = std::vector<CrtcConnectorPair>;

// Helper function that finds the property with the specified name.
bool GetDrmPropertyForName(DrmWrapper* drm,
                           drmModeObjectProperties* properties,
                           const std::string& name,
                           DrmWrapper::Property* property);

// If the |property| has a valid ID add it to the |property_set| request.
bool AddPropertyIfValid(drmModeAtomicReq* property_set,
                        uint32_t object_id,
                        const DrmWrapper::Property& property);

// Transforms the gamma curve into the drm_color_lut format with `size` entries.
ScopedDrmColorLutPtr CreateLutBlob(const display::GammaCurve& source,
                                   size_t size);

// Parse a Lut blob to retrieve a display::GammaCurve.
bool ParseLutBlob(const void* data, size_t size, display::GammaCurve& result);

// Converts |color_matrix| to a drm_color_ctm in U31.32 format where the most
// significant bit is the sign. If `negative_values_broken` is true, then
// clamp all negative values to 0.
ScopedDrmColorCtmPtr CreateCTMBlob(const skcms_Matrix3x3& color_matrix,
                                   bool negative_values_broken);

// Parse a CTM blob to retrieve a skcms_Matrix3x3.
bool ParseCTMBlob(const void* data, size_t size, skcms_Matrix3x3& result);

// Creates a FB Damage Clip Blob
ScopedDrmModeRectPtr CreateDCBlob(const gfx::Rect& rect);

// Returns the display infos parsed in
// |GetDisplayInfosAndInvalidCrtcs| and disables the invalid CRTCs
// that weren't picked as preferred CRTCs.
std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
GetDisplayInfosAndUpdateCrtcs(DrmWrapper& drm);

void DrmWriteIntoTraceHelper(const drmModeModeInfo& mode_info,
                             perfetto::TracedValue context);

// Given a list of connectors from |controllers_params|, generate a list of all
// possible CRTC-connector combinations. The returned vector will contain
// permutations that have all the connectors in connectors paired up with each
// of their supported CRTC.
std::vector<CrtcConnectorPairs> GetAllCrtcConnectorPermutations(
    const DrmDevice& drm,
    const std::vector<ControllerConfigParams>& controllers_params);

// Apply the color space conversion of `crtc_id` in-place on the specified RGB
// triple.
void ApplyCrtcColorSpaceConversion(DrmWrapper* drm,
                                   uint32_t crtc_id,
                                   float rgb[3]);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

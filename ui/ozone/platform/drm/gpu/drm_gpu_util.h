// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

namespace ui {

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

// Converts |color_matrix| to a drm_color_ctm in U31.32 format where the most
// significant bit is the sign.
// |color_matrix| represents a 3x3 matrix in vector form.
ScopedDrmColorCtmPtr CreateCTMBlob(const std::vector<float>& color_matrix);

// Creates a FB Damage Clip Blob
ScopedDrmModeRectPtr CreateDCBlob(const gfx::Rect& rect);

// Returns the display infos parsed in
// |GetDisplayInfosAndInvalidCrtcs| and disables the invalid CRTCs
// that weren't picked as preferred CRTCs.
HardwareDisplayControllerInfoList GetDisplayInfosAndUpdateCrtcs(
    DrmWrapper& drm);

void DrmWriteIntoTraceHelper(const drmModeModeInfo& mode_info,
                             perfetto::TracedValue context);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

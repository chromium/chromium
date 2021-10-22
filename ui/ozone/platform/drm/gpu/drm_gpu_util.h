// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

// Helper function that finds the property with the specified name.
bool GetDrmPropertyForName(DrmDevice* drm,
                           drmModeObjectProperties* properties,
                           const std::string& name,
                           DrmDevice::Property* property);

// If the |property| has a valid ID add it to the |property_set| request.
bool AddPropertyIfValid(drmModeAtomicReq* property_set,
                        uint32_t object_id,
                        const DrmDevice::Property& property);

// Transforms the gamma ramp entries into the drm_color_lut format.
ScopedDrmColorLutPtr CreateLutBlob(
    const std::vector<display::GammaRampRGBEntry>& source);

// Converts |color_matrix| to a drm_color_ctm in U31.32 format where the most
// significant bit is the sign.
// |color_matrix| represents a 3x3 matrix in vector form.
ScopedDrmColorCtmPtr CreateCTMBlob(const std::vector<float>& color_matrix);

// Creates a new look-up table of the desired size to fit the expectations of
// the DRM driver.
std::vector<display::GammaRampRGBEntry> ResampleLut(
    const std::vector<display::GammaRampRGBEntry>& lut_in,
    size_t desired_size);

// Check DRM driver name match.
bool IsDriverName(const char* device_file_name, const char* driver);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_H_

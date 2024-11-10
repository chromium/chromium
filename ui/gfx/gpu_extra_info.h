// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_EXTRA_INFO_H_
#define UI_GFX_GPU_EXTRA_INFO_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {

// Specification of a feature that can be enabled/disable in ANGLE
struct COMPONENT_EXPORT(GFX) ANGLEFeature {
  ANGLEFeature();
  ANGLEFeature(const ANGLEFeature& other);
  ANGLEFeature(ANGLEFeature&& other);
  ~ANGLEFeature();
  ANGLEFeature& operator=(const ANGLEFeature& other);
  ANGLEFeature& operator=(ANGLEFeature&& other);

  // Name of the feature in camel_case.
  std::string name;

  // Name of the category that the feature belongs to.
  std::string category;

  // Status, can be "enabled" or "disabled".
  std::string status;
};
using ANGLEFeatures = std::vector<ANGLEFeature>;

struct COMPONENT_EXPORT(GFX) GpuExtraInfo {
  GpuExtraInfo();
  GpuExtraInfo(const GpuExtraInfo&);
  GpuExtraInfo(GpuExtraInfo&&);
  ~GpuExtraInfo();
  GpuExtraInfo& operator=(const GpuExtraInfo&);
  GpuExtraInfo& operator=(GpuExtraInfo&&);

  // List of the currently available ANGLE features. May be empty if not
  // applicable.
  ANGLEFeatures angle_features;

#if BUILDFLAG(IS_OZONE_X11)
  std::vector<gfx::BufferUsageAndFormat> gpu_memory_buffer_support_x11;
#endif  // BUILDFLAG(IS_OZONE_X11)
};

}  // namespace gfx

#endif  // UI_GFX_GPU_EXTRA_INFO_H_

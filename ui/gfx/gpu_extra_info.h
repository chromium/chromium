// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_EXTRA_INFO_H_
#define UI_GFX_GPU_EXTRA_INFO_H_

#include <string>
#include <vector>

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gfx_export.h"

#if defined(USE_OZONE)
#include "ui/ozone/buildflags.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_OZONE_PLATFORM_X11
#endif
#endif

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
#include "ui/gfx/x/xproto.h"
#endif

namespace gfx {

// Specification of a feature that can be enabled/disable in ANGLE
struct GFX_EXPORT ANGLEFeature {
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

  // One sentence description of the feature, why it's available.
  std::string description;

  // Full link to cr/angle bug if applicable.
  std::string bug;

  // Status, can be "enabled" or "disabled".
  std::string status;

  // Condition, contains the condition that set 'status'.
  std::string condition;
};
using ANGLEFeatures = std::vector<ANGLEFeature>;

struct GFX_EXPORT GpuExtraInfo {
  GpuExtraInfo();
  GpuExtraInfo(const GpuExtraInfo&);
  GpuExtraInfo(GpuExtraInfo&&);
  ~GpuExtraInfo();
  GpuExtraInfo& operator=(const GpuExtraInfo&);
  GpuExtraInfo& operator=(GpuExtraInfo&&);

  // List of the currently available ANGLE features. May be empty if not
  // applicable.
  ANGLEFeatures angle_features;

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
  x11::VisualId system_visual{};
  x11::VisualId rgba_visual{};

  std::vector<gfx::BufferUsageAndFormat> gpu_memory_buffer_support_x11;
#endif
};

}  // namespace gfx

#endif  // UI_GFX_GPU_EXTRA_INFO_H_

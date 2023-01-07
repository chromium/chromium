// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_extra_info.h"

namespace gfx {

ANGLEFeature::ANGLEFeature() = default;
ANGLEFeature::ANGLEFeature(const ANGLEFeature& other) = default;
ANGLEFeature::ANGLEFeature(ANGLEFeature&& other) = default;
ANGLEFeature::~ANGLEFeature() = default;
ANGLEFeature& ANGLEFeature::operator=(const ANGLEFeature& other) = default;
ANGLEFeature& ANGLEFeature::operator=(ANGLEFeature&& other) = default;

GpuExtraInfo::GpuExtraInfo() = default;
GpuExtraInfo::GpuExtraInfo(const GpuExtraInfo&) = default;
GpuExtraInfo::GpuExtraInfo(GpuExtraInfo&&) = default;
GpuExtraInfo::~GpuExtraInfo() = default;
GpuExtraInfo& GpuExtraInfo::operator=(const GpuExtraInfo&) = default;
GpuExtraInfo& GpuExtraInfo::operator=(GpuExtraInfo&&) = default;

}  // namespace gfx

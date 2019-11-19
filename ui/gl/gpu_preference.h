// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_PREFERENCE_H_
#define UI_GL_GPU_PREFERENCE_H_

namespace gl {

// On dual-GPU systems, expresses a preference for using the low power
// or high performance GPU. On systems that have dual-GPU support (see
// GpuDataManagerImpl), resource sharing only works between
// contexts that are created with the same GPU preference.
//
// This API will likely need to be adjusted as the functionality is
// implemented on more operating systems.
enum class GpuPreference {
  kDefault,
  kLowPower,
  kHighPerformance,
  kMaxValue = kHighPerformance
};

}  // namespace gl

#endif  // UI_GL_GPU_PREFERENCE_H_

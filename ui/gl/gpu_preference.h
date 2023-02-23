// Copyright 2012 The Chromium Authors
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
  kNone,
  kDefault,
  kLowPower,
  kHighPerformance,
  kMaxValue = kHighPerformance
};

// Some clients may need to use the same GPU with a separate EGL display.
// This enum is used to key individual EGL displays per-GPU.
enum class DisplayKey {
  kDefault,
  kSeparateEGLDisplayForWebGLTesting,
  kMaxValue = kSeparateEGLDisplayForWebGLTesting,
};

}  // namespace gl

#endif  // UI_GL_GPU_PREFERENCE_H_

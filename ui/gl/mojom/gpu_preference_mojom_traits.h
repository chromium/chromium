// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_
#define UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_

#include "ui/gl/gpu_preference.h"
#include "ui/gl/mojom/gpu_preference.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gl::mojom::GpuPreference, gl::GpuPreference> {
  static gl::mojom::GpuPreference ToMojom(gl::GpuPreference preference) {
    switch (preference) {
      case gl::GpuPreference::kDefault:
        return gl::mojom::GpuPreference::kDefault;
      case gl::GpuPreference::kLowPower:
        return gl::mojom::GpuPreference::kLowPower;
      case gl::GpuPreference::kHighPerformance:
        return gl::mojom::GpuPreference::kHighPerformance;
    }
    NOTREACHED();
    return gl::mojom::GpuPreference::kDefault;
  }

  static bool FromMojom(gl::mojom::GpuPreference input,
                        gl::GpuPreference* out) {
    switch (input) {
      case gl::mojom::GpuPreference::kDefault:
        *out = gl::GpuPreference::kDefault;
        return true;
      case gl::mojom::GpuPreference::kLowPower:
        *out = gl::GpuPreference::kLowPower;
        return true;
      case gl::mojom::GpuPreference::kHighPerformance:
        *out = gl::GpuPreference::kHighPerformance;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_

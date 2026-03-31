// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_
#define UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/mojom/gpu_preference.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gl::mojom::GpuPreference, gl::GpuPreference> {
  static gl::mojom::GpuPreference ToMojom(gl::GpuPreference preference) {
    switch (preference) {
      case gl::GpuPreference::kNone:
        return gl::mojom::GpuPreference::kNone;
      case gl::GpuPreference::kDefault:
        return gl::mojom::GpuPreference::kDefault;
      case gl::GpuPreference::kLowPower:
        return gl::mojom::GpuPreference::kLowPower;
      case gl::GpuPreference::kHighPerformance:
        return gl::mojom::GpuPreference::kHighPerformance;
    }
    NOTREACHED();
  }

  static gl::GpuPreference FromMojom(gl::mojom::GpuPreference input) {
    switch (input) {
      case gl::mojom::GpuPreference::kNone:
        return gl::GpuPreference::kNone;
      case gl::mojom::GpuPreference::kDefault:
        return gl::GpuPreference::kDefault;
      case gl::mojom::GpuPreference::kLowPower:
        return gl::GpuPreference::kLowPower;
      case gl::mojom::GpuPreference::kHighPerformance:
        return gl::GpuPreference::kHighPerformance;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_GL_MOJOM_GPU_PREFERENCE_MOJOM_TRAITS_H_

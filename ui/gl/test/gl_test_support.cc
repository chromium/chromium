// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_support.h"

#include <array>
#include <vector>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/config/gpu_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_OZONE)
#include "base/run_loop.h"
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gl {

namespace {

template <typename T>
void rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b, T* y, T* u, T* v) {
  // These values are used in the transformation from YUV to RGB color values.
  // They are taken from http://www.fourcc.org/fccyvrgb.php
  *y = (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
  *u = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
  *v = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
}

UNSAFE_BUFFER_USAGE base::span<uint8_t> ToSpan_uint8(uint8_t* data,
                                                     size_t start,
                                                     size_t size) {
  return UNSAFE_BUFFERS(base::span<uint8_t>(data + start, size));
}

UNSAFE_BUFFER_USAGE base::span<uint16_t> ToSpan_uint16(uint8_t* data,
                                                       size_t start,
                                                       size_t size) {
  uint16_t* pointer = reinterpret_cast<uint16_t*>(UNSAFE_BUFFERS(data + start));
  return UNSAFE_BUFFERS(base::span<uint16_t>(pointer, size));
}

UNSAFE_BUFFER_USAGE base::span<uint32_t> ToSpan_uint32(uint8_t* data,
                                                       size_t start,
                                                       size_t size) {
  uint32_t* pointer = reinterpret_cast<uint32_t*>(UNSAFE_BUFFERS(data + start));
  return UNSAFE_BUFFERS(base::span<uint32_t>(pointer, size));
}

UNSAFE_BUFFER_USAGE base::span<uint64_t> ToSpan_uint64(uint8_t* data,
                                                       size_t start,
                                                       size_t size) {
  uint64_t* pointer = reinterpret_cast<uint64_t*>(UNSAFE_BUFFERS(data + start));
  return UNSAFE_BUFFERS(base::span<uint64_t>(pointer, size));
}

}  // namespace

// static
GLDisplay* GLTestSupport::InitializeGL(
    std::optional<GLImplementationParts> prefered_impl) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  gpu::TrySetNonSoftwareDevicePreferenceForTesting(
      gl::GpuPreference ::kDefault);
#endif

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  std::vector<GLImplementationParts> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementationParts impl =
      prefered_impl ? *prefered_impl : allowed_impls[0];
  DCHECK(impl.IsAllowed(allowed_impls));

  GLDisplay* display =
      GLSurfaceTestSupport::InitializeOneOffImplementation(impl);
#if BUILDFLAG(IS_OZONE)
  // Make sure all the tasks posted to the current task runner by the
  // initialization functions are run before running the tests.
  base::RunLoop().RunUntilIdle();
#endif
  return display;
}

// static
void GLTestSupport::CleanupGL(GLDisplay* display) {
  GLSurfaceTestSupport::ShutdownGL(display);
}

// static
void GLTestSupport::SetBufferDataToColor(int width,
                                         int height,
                                         int stride,
                                         int plane,
                                         gfx::BufferFormat format,
                                         base::span<const uint8_t, 4> color,
                                         uint8_t* data) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width));
        std::ranges::fill(row, color[0]);  // R
      }
      return;
    case gfx::BufferFormat::RG_88:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        std::ranges::fill(row, (color[1] << 8) |  // G
                                   color[0]);     // R
      }
      return;
    case gfx::BufferFormat::R_16:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        std::ranges::fill(row, color[0] << 8);  // R
      }
      return;
    case gfx::BufferFormat::RG_1616:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(row, (color[1] << 24) |     // G
                                   (color[0] << 8));  // R
      }
      return;
    case gfx::BufferFormat::RGBA_4444:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        std::ranges::fill(row, ((color[3] & 0xf) << 4) |      // A
                                   (color[2] & 0xf) |         // B
                                   (color[1] << 12) |         // G
                                   ((color[0] & 0xf) << 8));  // R
      }
      return;
    case gfx::BufferFormat::BGR_565:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        std::ranges::fill(row, ((color[0] >> 3) << 11) |     // R
                                   ((color[1] >> 2) << 5) |  // G
                                   (color[2] >> 3));         // B
      }
      return;
    case gfx::BufferFormat::RGBX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(row, (0xaa << 24) |          // unused
                                   (color[2] << 16) |  // B
                                   (color[1] << 8) |   // G
                                   color[0]);          // R
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(row, (color[3] << 24) |      // A
                                   (color[2] << 16) |  // B
                                   (color[1] << 8) |   // G
                                   color[0]);          // R
      }
      return;
    case gfx::BufferFormat::BGRX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(row, (0xaa << 24) |          // unused
                                   (color[0] << 16) |  // R
                                   (color[1] << 8) |   // G
                                   color[2]);          // B
      }
      return;
    case gfx::BufferFormat::BGRA_1010102: {
      DCHECK_EQ(0, plane);
      DCHECK_EQ(63, color[3] % 64) << "Alpha channel doesn't have enough "
                                      "precision for the supplied value";
      const uint8_t scaled_alpha = color[3] >> 6;
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(
            row,
            (scaled_alpha << 30) |                             // A
                (((color[0] << 2) | (color[0] >> 6)) << 20) |  // R
                (((color[1] << 2) | (color[1] >> 6)) << 10) |  // G
                ((color[2] << 2) | (color[2] >> 6)));          // B
      }
      return;
    }
    case gfx::BufferFormat::RGBA_1010102: {
      DCHECK_EQ(0, plane);
      DCHECK_EQ(63, color[3] % 64) << "Alpha channel doesn't have enough "
                                      "precision for the supplied value";
      const uint8_t scaled_alpha = color[3] >> 6;
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(
            row, (scaled_alpha << 30) |                             // A
                     (((color[2] << 2) | (color[2] >> 6)) << 20) |  // B
                     (((color[1] << 2) | (color[1] >> 6)) << 10) |  // G
                     ((color[0] << 2) | (color[0] >> 6)));          // R
      }
      return;
    }
    case gfx::BufferFormat::BGRA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        std::ranges::fill(row, (color[3] << 24) |      // A
                                   (color[0] << 16) |  // R
                                   (color[1] << 8) |   // G
                                   color[2]);          // B
      }
      return;
    case gfx::BufferFormat::RGBA_F16: {
      DCHECK_EQ(0, plane);
      float float_color[4] = {
          color[0] / 255.f,  // R
          color[1] / 255.f,  // G
          color[2] / 255.f,  // B
          color[3] / 255.f,  // A
      };
      uint16_t half_float_color[4];
      gfx::FloatToHalfFloat(float_color, half_float_color, 4);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint64(data, y * stride, width));
        std::ranges::fill(row, base::bit_cast<uint64_t>(half_float_color));
      }
      return;
    }
    case gfx::BufferFormat::YVU_420: {
      DCHECK_LT(plane, 3);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      std::array<uint8_t, 3> yvu = {};
      rgb_to_yuv(color[0], color[1], color[2], &yvu[0], &yvu[2], &yvu[1]);

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          std::ranges::fill(row, yvu[0]);
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width / 2));
          std::ranges::fill(row, yvu[plane]);
        }
      }
      return;
    }
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      DCHECK_LT(plane, 2);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      std::array<uint8_t, 3> yuv = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          std::ranges::fill(row, yuv[0]);
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint16(data, stride * y, width / 2));
          std::ranges::fill(row, (yuv[2] << 8) | yuv[1]);
        }
      }
      return;
    }
    case gfx::BufferFormat::YUVA_420_TRIPLANAR: {
      DCHECK_LT(plane, 3);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      std::array<uint8_t, 4> yuv = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);
      yuv[3] = color[3];

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          std::ranges::fill(row, yuv[0]);
        }
      } else if (plane == 1) {
        for (int y = 0; y < height / 2; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint16(data, stride * y, width / 2));
          std::ranges::fill(row, (yuv[2] << 8) | yuv[1]);
        }
      } else {
        for (int y = 0; y < height; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          std::ranges::fill(row, yuv[3]);
        }
      }
      return;
    }
    case gfx::BufferFormat::P010: {
      DCHECK_LT(plane, 3);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      std::array<uint16_t, 3> yuv = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
          std::ranges::fill(row, yuv[0] << 2);
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width / 2));
          std::ranges::fill(row, (yuv[2] << 18) | (yuv[1] << 2));
        }
      }
      return;
    }
  }
  NOTREACHED();
}

}  // namespace gl

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_support.h"

#include <array>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
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

}  // namespace

// static
GLDisplay* GLTestSupport::InitializeGL(
    std::optional<GLImplementationParts> prefered_impl) {
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
        std::ranges::fill(row, color[0]);
      }
      return;
    case gfx::BufferFormat::RG_88:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 2));
        std::ranges::fill(row, color[0]);
      }
      return;
    case gfx::BufferFormat::R_16:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        for (int x = 0; x < width; ++x) {
          row[x] = static_cast<uint16_t>(color[0] << 8);
        }
      }
      return;
    case gfx::BufferFormat::RG_1616:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width * 2));
        for (int x = 0; x < width; ++x) {
          row[2 * x + 0] = static_cast<uint16_t>(color[0] << 8);
          row[2 * x + 1] = static_cast<uint16_t>(color[1] << 8);
        }
      }
      return;
    case gfx::BufferFormat::RGBA_4444:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 2));
        for (int x = 0; x < width; ++x) {
          row[x * 2 + 0] = (color[1] << 4) | (color[0] & 0xf);
          row[x * 2 + 1] = (color[3] << 4) | (color[2] & 0xf);
        }
      }
      return;
    case gfx::BufferFormat::BGR_565:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
        for (int x = 0; x < width; ++x) {
          row[x] = ((color[0] >> 3) << 11) | ((color[1] >> 2) << 5) |
                   (color[2] >> 3);
        }
      }
      return;
    case gfx::BufferFormat::RGBX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 4));
        for (int x = 0; x < width; ++x) {
          row[x * 4 + 0] = color[0];
          row[x * 4 + 1] = color[1];
          row[x * 4 + 2] = color[2];
          row[x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 4));
        for (int x = 0; x < width; ++x) {
          row[x * 4 + 0] = color[0];
          row[x * 4 + 1] = color[1];
          row[x * 4 + 2] = color[2];
          row[x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::BGRX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 4));
        for (int x = 0; x < width; ++x) {
          row[x * 4 + 0] = color[2];
          row[x * 4 + 1] = color[1];
          row[x * 4 + 2] = color[0];
          row[x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::BGRA_1010102: {
      DCHECK_EQ(0, plane);
      DCHECK_EQ(63, color[3] % 64) << "Alpha channel doesn't have enough "
                                      "precision for the supplied value";
      const uint8_t scaled_alpha = color[3] >> 6;
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint32(data, y * stride, width));
        for (int x = 0; x < width; ++x) {
          row[x] = (scaled_alpha << 30) |                       // A
                   ((color[0] << 2) | (color[0] >> 6)) << 20 |  // B
                   ((color[1] << 2) | (color[1] >> 6)) << 10 |  // G
                   ((color[2] << 2) | (color[2] >> 6));         // R
        }
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
        for (int x = 0; x < width; ++x) {
          row[x] = (scaled_alpha << 30) |                       // A
                   ((color[2] << 2) | (color[2] >> 6)) << 20 |  // B
                   ((color[1] << 2) | (color[1] >> 6)) << 10 |  // G
                   ((color[0] << 2) | (color[0] >> 6));         // R
        }
      }
      return;
    }
    case gfx::BufferFormat::BGRA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint8(data, y * stride, width * 4));
        for (int x = 0; x < width; ++x) {
          row[x * 4 + 0] = color[2];
          row[x * 4 + 1] = color[1];
          row[x * 4 + 2] = color[0];
          row[x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::RGBA_F16: {
      DCHECK_EQ(0, plane);
      float float_color[4] = {
          color[0] / 255.f,
          color[1] / 255.f,
          color[2] / 255.f,
          color[3] / 255.f,
      };
      uint16_t half_float_color[4];
      gfx::FloatToHalfFloat(float_color, half_float_color, 4);
      for (int y = 0; y < height; ++y) {
        auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width * 4));
        for (int x = 0; x < width; ++x) {
          row[x * 4 + 0] = half_float_color[0];
          row[x * 4 + 1] = half_float_color[1];
          row[x * 4 + 2] = half_float_color[2];
          row[x * 4 + 3] = half_float_color[3];
        }
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
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          for (int x = 0; x < width / 2; ++x) {
            row[x * 2] = yuv[1];
            row[x * 2 + 1] = yuv[2];
          }
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
          auto row = UNSAFE_TODO(ToSpan_uint8(data, stride * y, width));
          for (int x = 0; x < width / 2; ++x) {
            row[x * 2] = yuv[1];
            row[x * 2 + 1] = yuv[2];
          }
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
          auto row = UNSAFE_TODO(ToSpan_uint16(data, y * stride, width));
          for (int x = 0; x < width; x += 2) {
            row[x] = yuv[1] << 2;
            row[x + 1] = yuv[2] << 2;
          }
        }
      }
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}
}  // namespace gl

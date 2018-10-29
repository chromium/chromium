// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_image_test_support.h"

#include <vector>

#include "base/stl_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(USE_OZONE)
#include "base/run_loop.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gl {

// static
void GLImageTestSupport::InitializeGL(
    base::Optional<GLImplementation> prefered_impl) {
#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.using_mojo = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  std::vector<GLImplementation> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementation impl = prefered_impl ? *prefered_impl : allowed_impls[0];
  DCHECK(base::ContainsValue(allowed_impls, impl));

  GLSurfaceTestSupport::InitializeOneOffImplementation(impl, true);
#if defined(USE_OZONE)
  // Make sure all the tasks posted to the current task runner by the
  // initialization functions are run before running the tests.
  base::RunLoop().RunUntilIdle();
#endif
}

// static
void GLImageTestSupport::CleanupGL() {
  init::ShutdownGL(false);
}

// static
void GLImageTestSupport::SetBufferDataToColor(int width,
                                              int height,
                                              int stride,
                                              int plane,
                                              gfx::BufferFormat format,
                                              const uint8_t color[4],
                                              uint8_t* data) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        memset(&data[y * stride], color[0], width);
      }
      return;
    case gfx::BufferFormat::R_16:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        uint16_t* row = reinterpret_cast<uint16_t*>(data + y * stride);
        for (int x = 0; x < width; ++x) {
          row[x] = static_cast<uint16_t>(color[0] << 8);
        }
      }
      return;
    case gfx::BufferFormat::BGR_565:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          *reinterpret_cast<uint16_t*>(&data[y * stride + x * 2]) =
              ((color[2] >> 3) << 11) | ((color[1] >> 2) << 5) |
              (color[0] >> 3);
        }
      }
      return;
    case gfx::BufferFormat::RGBX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[0];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[2];
          data[y * stride + x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[0];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[2];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::BGRX_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[2];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[0];
          data[y * stride + x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::BGRX_1010102:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          *reinterpret_cast<uint32_t*>(&data[y * stride + x * 4]) =
              0x3 << 30 |  // Alpha channel is unused
              ((color[0] << 2) | (color[0] >> 6)) << 20 |  // R
              ((color[1] << 2) | (color[1] >> 6)) << 10 |  // G
              ((color[2] << 2) | (color[2] >> 6));         // B
        }
      }
      return;

    case gfx::BufferFormat::RGBX_1010102:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          *reinterpret_cast<uint32_t*>(&data[y * stride + x * 4]) =
              0x3 << 30 |  // Alpha channel is unused
              ((color[2] << 2) | (color[2] >> 6)) << 20 |  // B
              ((color[1] << 2) | (color[1] >> 6)) << 10 |  // G
              ((color[0] << 2) | (color[0] >> 6));         // R
        }
      }
      return;

    case gfx::BufferFormat::BGRA_8888:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[2];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[0];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::RGBA_F16: {
      DCHECK_EQ(0, plane);
      float float_color[4] = {
          color[0] / 255.f, color[1] / 255.f, color[2] / 255.f,
          color[3] / 255.f,
      };
      uint16_t half_float_color[4];
      gfx::FloatToHalfFloat(float_color, half_float_color, 4);
      for (int y = 0; y < height; ++y) {
        uint16_t* row = reinterpret_cast<uint16_t*>(data + y * stride);
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
      // These values are used in the transformation from YUV to RGB color
      // values. They are taken from the following webpage:
      // http://www.fourcc.org/fccyvrgb.php
      uint8_t yvu[] = {
          (0.257 * color[0]) + (0.504 * color[1]) + (0.098 * color[2]) + 16,
          (0.439 * color[0]) - (0.368 * color[1]) - (0.071 * color[2]) + 128,
          -(0.148 * color[0]) - (0.291 * color[1]) + (0.439 * color[2]) + 128};
      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            data[stride * y + x] = yvu[0];
          }
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          for (int x = 0; x < width / 2; ++x) {
            data[stride * y + x] = yvu[plane];
          }
        }
      }
      return;
    }
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      DCHECK_LT(plane, 2);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      // These values are used in the transformation from YUV to RGB color
      // values. They are taken from the following webpage:
      // http://www.fourcc.org/fccyvrgb.php
      uint8_t yuv[] = {
          (0.257 * color[0]) + (0.504 * color[1]) + (0.098 * color[2]) + 16,
          -(0.148 * color[0]) - (0.291 * color[1]) + (0.439 * color[2]) + 128,
          (0.439 * color[0]) - (0.368 * color[1]) - (0.071 * color[2]) + 128};
      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            data[stride * y + x] = yuv[0];
          }
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          for (int x = 0; x < width / 2; ++x) {
            data[stride * y + x * 2] = yuv[1];
            data[stride * y + x * 2 + 1] = yuv[2];
          }
        }
      }
      return;
    }
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return;
  }
  NOTREACHED();
}

}  // namespace gl

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_support.h"

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

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif
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
}  // namespace

// static
GLDisplay* GLTestSupport::InitializeGL(
    absl::optional<GLImplementationParts> prefered_impl) {
#if BUILDFLAG(IS_OZONE)
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(b/271455200): is the process single-threaded at this point and has
  // the FeatureList been initialized? Those are requirements of
  // ui::EnsureIntelMediaCompressionEnvVarIsSet().
  ui::EnsureIntelMediaCompressionEnvVarIsSet();
#endif
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
      GLSurfaceTestSupport::InitializeOneOffImplementation(impl, true);
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
    case gfx::BufferFormat::RG_1616:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        uint16_t* row = reinterpret_cast<uint16_t*>(data + y * stride);
        for (int x = 0; x < width; ++x) {
          row[2 * x + 0] = static_cast<uint16_t>(color[0] << 8);
          row[2 * x + 1] = static_cast<uint16_t>(color[1] << 8);
        }
      }
      return;
    case gfx::BufferFormat::RGBA_4444:
      DCHECK_EQ(0, plane);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 2 + 0] = (color[1] << 4) | (color[0] & 0xf);
          data[y * stride + x * 2 + 1] = (color[3] << 4) | (color[2] & 0xf);
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
    case gfx::BufferFormat::BGRA_1010102: {
      DCHECK_EQ(0, plane);
      DCHECK_EQ(63, color[3] % 64) << "Alpha channel doesn't have enough "
                                      "precision for the supplied value";
      const uint8_t scaled_alpha = color[3] >> 6;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          *reinterpret_cast<uint32_t*>(&data[y * stride + x * 4]) =
              (scaled_alpha << 30) |                       // A
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
        for (int x = 0; x < width; ++x) {
          *reinterpret_cast<uint32_t*>(&data[y * stride + x * 4]) =
              (scaled_alpha << 30) |                       // A
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
          color[0] / 255.f,
          color[1] / 255.f,
          color[2] / 255.f,
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
      uint8_t yvu[3] = {};
      rgb_to_yuv(color[0], color[1], color[2], &yvu[0], &yvu[2], &yvu[1]);

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
      uint8_t yuv[3] = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);

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
    case gfx::BufferFormat::YUVA_420_TRIPLANAR: {
      DCHECK_LT(plane, 3);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      uint8_t yuv[4] = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);
      yuv[3] = color[3];

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            data[stride * y + x] = yuv[0];
          }
        }
      } else if (plane == 1) {
        for (int y = 0; y < height / 2; ++y) {
          for (int x = 0; x < width / 2; ++x) {
            data[stride * y + x * 2] = yuv[1];
            data[stride * y + x * 2 + 1] = yuv[2];
          }
        }
      } else {
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            data[stride * y + x] = yuv[3];
          }
        }
      }
      return;
    }
    case gfx::BufferFormat::P010: {
      DCHECK_LT(plane, 3);
      DCHECK_EQ(0, height % 2);
      DCHECK_EQ(0, width % 2);
      uint16_t yuv[3] = {};
      rgb_to_yuv(color[0], color[1], color[2], &yuv[0], &yuv[1], &yuv[2]);

      if (plane == 0) {
        for (int y = 0; y < height; ++y) {
          uint16_t* row = reinterpret_cast<uint16_t*>(data + y * stride);
          for (int x = 0; x < width; ++x) {
            row[x] = yuv[0] << 2;
          }
        }
      } else {
        for (int y = 0; y < height / 2; ++y) {
          uint16_t* row = reinterpret_cast<uint16_t*>(data + y * stride);
          for (int x = 0; x < width; x += 2) {
            row[x] = yuv[1] << 2;
            row[x + 1] = yuv[2] << 2;
          }
        }
      }
      return;
    }
  }
  NOTREACHED();
}
}  // namespace gl

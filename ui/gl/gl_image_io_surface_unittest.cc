// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gl {
namespace {

const uint8_t kImageColor[] = {0x30, 0x40, 0x10, 0xFF};

template <gfx::BufferFormat format>
class GLImageIOSurfaceTestDelegate : public GLImageTestDelegateBase {
 public:
  scoped_refptr<GLImage> CreateImage(const gfx::Size& size) const {
    scoped_refptr<GLImageIOSurface> image(GLImageIOSurface::Create(
        size, GLImageIOSurface::GetInternalFormatForTesting(format)));
    IOSurfaceRef surface_ref = gfx::CreateIOSurface(size, format);
    bool rv =
        image->Initialize(surface_ref, gfx::GenericSharedMemoryId(1), format);
    EXPECT_TRUE(rv);
    return image;
  }

  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    scoped_refptr<GLImageIOSurface> image(GLImageIOSurface::Create(
        size, GLImageIOSurface::GetInternalFormatForTesting(format)));
    IOSurfaceRef surface_ref = gfx::CreateIOSurface(size, format);
    IOReturn status = IOSurfaceLock(surface_ref, 0, nullptr);
    EXPECT_NE(status, kIOReturnCannotLock);

    uint8_t corrected_color[4];
    if (format == gfx::BufferFormat::RGBA_8888) {
      // GL_RGBA is not supported by CGLTexImageIOSurface2D(), so we pretend it
      // is GL_BGRA, (see https://crbug.com/533677#c6) swizzle the channels for
      // the purpose of this test.
      corrected_color[0] = color[2];
      corrected_color[1] = color[1];
      corrected_color[2] = color[0];
      corrected_color[3] = color[3];
    } else {
      memcpy(corrected_color, color, base::size(corrected_color));
    }

    for (size_t plane = 0; plane < NumberOfPlanesForLinearBufferFormat(format);
         ++plane) {
      void* data = IOSurfaceGetBaseAddressOfPlane(surface_ref, plane);
      GLImageTestSupport::SetBufferDataToColor(
          size.width(), size.height(),
          IOSurfaceGetBytesPerRowOfPlane(surface_ref, plane), plane, format,
          corrected_color, static_cast<uint8_t*>(data));
    }
    IOSurfaceUnlock(surface_ref, 0, nullptr);

    bool rv =
        image->Initialize(surface_ref, gfx::GenericSharedMemoryId(1), format);
    EXPECT_TRUE(rv);

    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_RECTANGLE_ARB; }

  const uint8_t* GetImageColor() {
    if (format != gfx::BufferFormat::BGRX_8888)
      return kImageColor;

    // BGRX_8888 is actually treated as BGRA because many operations are broken
    // when binding an IOSurface as GL_RGB, see https://crbug.com/595948. This
    // makes the alpha value comparison fail, because we expect 0xFF but are
    // actually writing something (0xAA). Correct the alpha value for the test.
    static uint8_t bgrx_image_color[] = {kImageColor[0], kImageColor[1],
                                         kImageColor[2], 0xAA};
    return bgrx_image_color;
  }

  int GetAdmissibleError() const {
    return format == gfx::BufferFormat::YUV_420_BIPLANAR ? 1 : 0;
  }
};

using GLImageTestTypes = testing::Types<
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_F16>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_1010102>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageIOSurface, GLImageTest, GLImageTestTypes);

using GLImageRGBTestTypes = testing::Types<
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_F16>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_1010102>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageIOSurface,
                               GLImageZeroInitializeTest,
                               GLImageRGBTestTypes);

using GLImageBindTestTypes = testing::Types<
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_8888>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::RGBA_F16>,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::BGRX_1010102>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageIOSurface,
                               GLImageBindTest,
                               GLImageBindTestTypes);

INSTANTIATE_TYPED_TEST_SUITE_P(
    GLImageIOSurface,
    GLImageCopyTest,
    GLImageIOSurfaceTestDelegate<gfx::BufferFormat::YUV_420_BIPLANAR>);

}  // namespace
}  // namespace gl

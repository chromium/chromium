// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/test/gl_image_test_template.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace {

const uint8_t kRed[] = {0xF0, 0x0, 0x0, 0xFF};
const uint8_t kYellow[] = {0xF0, 0xFF, 0x00, 0xFF};

template <gfx::BufferUsage usage, gfx::BufferFormat format>
class GLImageNativePixmapTestDelegate : public GLImageTestDelegateBase {
 public:
  GLImageNativePixmapTestDelegate() {
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  }

  ~GLImageNativePixmapTestDelegate() override = default;

  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<gfx::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget,
                                            nullptr, size, format, usage);
    DCHECK(pixmap) << "Offending format: " << gfx::BufferFormatToString(format);
    if (usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE ||
        usage == gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE ||
        usage == gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE) {
      auto client_pixmap = client_native_pixmap_factory_->ImportFromHandle(
          pixmap->ExportHandle(), size, format, usage);
      bool mapped = client_pixmap->Map();
      EXPECT_TRUE(mapped);

      for (size_t plane = 0; plane < pixmap->GetNumberOfPlanes(); ++plane) {
        void* data = client_pixmap->GetMemoryAddress(plane);
        GLImageTestSupport::SetBufferDataToColor(
            size.width(), size.height(), pixmap->GetDmaBufPitch(plane), plane,
            pixmap->GetBufferFormat(), color, static_cast<uint8_t*>(data));
      }
      client_pixmap->Unmap();
    }

    auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
    EXPECT_TRUE(image->Initialize(pixmap.get()));
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_EXTERNAL_OES; }

  const uint8_t* GetImageColor() const {
    return format == gfx::BufferFormat::R_8 ? kRed : kYellow;
  }

  int GetAdmissibleError() const {
    if (format == gfx::BufferFormat::YVU_420 ||
        format == gfx::BufferFormat::YUV_420_BIPLANAR) {
      return 1;
    }
    if (format == gfx::BufferFormat::P010)
      return 3;
    return 0;
  }

 private:
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLImageNativePixmapTestDelegate);
};

using GLImageScanoutType = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageNativePixmapScanoutBGRA,
                               GLImageTest,
                               GLImageScanoutType);

using GLImageScanoutTypeDisabled = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::RGBX_1010102>>;

// This test is disabled since we need mesa support for XR30/XB30 that is not
// available on many boards yet.
INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_GLImageNativePixmapScanoutRGBX,
                               GLImageTest,
                               GLImageScanoutTypeDisabled);

using GLImageReadWriteType = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::R_8>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::P010>>;

using GLImageBindTestTypes = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::BGRA_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::RGBX_1010102>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::R_8>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::YVU_420>,
    GLImageNativePixmapTestDelegate<
        gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferFormat::YVU_420>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageNativePixmapTestDelegate<
        gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::P010>>;

// These tests are disabled since the trybots are running with Ozone X11
// implementation that doesn't support creating ClientNativePixmap.
// TODO(dcastagna): Implement ClientNativePixmapFactory on Ozone X11.
INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_GLImageNativePixmapReadWrite,
                               GLImageTest,
                               GLImageReadWriteType);

INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_GLImageNativePixmap,
                               GLImageBindTest,
                               GLImageBindTestTypes);

}  // namespace
}  // namespace gl

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringize_macros.h"
#include "gpu/command_buffer/service/shared_image/gl_image_native_pixmap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_support.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

// TODO(crbug.com/969798): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_Create DISABLED_Create
#else
#define MAYBE_Create Create
#endif

// TYPED_TEST_P() and REGISTER_TYPED_TEST_SUITE_P() don't do macro expansion on
// their parameters, making the MAYBE_ technique above not work -- these macros
// are a workaround.
#define TYPED_TEST_P_WITH_EXPANSION(SuiteName, TestName) \
  TYPED_TEST_P(SuiteName, TestName)
#define REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(SuiteName, ...) \
  REGISTER_TYPED_TEST_SUITE_P(SuiteName, __VA_ARGS__)

namespace gl {
namespace {

const uint8_t kRed[] = {0xF0, 0x0, 0x0, 0xFF};
const uint8_t kYellow[] = {0xF0, 0xFF, 0x00, 0xFF};

template <typename GLImageTestDelegate>
class GLImageTest : public testing::Test {
 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    display_ = GLTestSupport::InitializeGL(absl::nullopt);
    surface_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), GLContextAttribs());
    context_->MakeCurrent(surface_.get());
  }
  void TearDown() override {
    delegate_.WillTearDown();
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    surface_ = nullptr;
    GLTestSupport::CleanupGL(display_);
  }

 protected:
  scoped_refptr<GLSurface> surface_;
  scoped_refptr<GLContext> context_;
  GLImageTestDelegate delegate_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TYPED_TEST_SUITE_P(GLImageTest);

TYPED_TEST_P_WITH_EXPANSION(GLImageTest, MAYBE_Create) {
  if (this->delegate_.SkipTest(this->display_)) {
    GTEST_SKIP() << "Skip because GL initialization failed";
  }

  // NOTE: On some drm devices (mediatek) the minimum width/height to add an fb
  // for a bo must be 64, and YVU_420 in i915 requires at least 128 length.
  const gfx::Size small_image_size(128, 128);
  const gfx::Size large_image_size(512, 512);
  const uint8_t* image_color = this->delegate_.GetImageColor();

  // Create a small solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<gpu::GLImageNativePixmap> small_image =
      this->delegate_.CreateSolidColorImage(small_image_size, image_color);
  ASSERT_TRUE(small_image);

  // Create a large solid color green image of preferred format. This must
  // succeed in order for a GLImage to be conformant.
  scoped_refptr<gpu::GLImageNativePixmap> large_image =
      this->delegate_.CreateSolidColorImage(large_image_size, image_color);
  ASSERT_TRUE(large_image);
}

// The GLImageTest test case verifies the behaviour that is expected from a
// GLImage in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P_WITH_EXPANSION(GLImageTest, MAYBE_Create);

template <gfx::BufferUsage usage, gfx::BufferFormat format>
class GLImageNativePixmapTestDelegate {
 public:
  GLImageNativePixmapTestDelegate() {
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  }

  GLImageNativePixmapTestDelegate(const GLImageNativePixmapTestDelegate&) =
      delete;
  GLImageNativePixmapTestDelegate& operator=(
      const GLImageNativePixmapTestDelegate&) = delete;

  ~GLImageNativePixmapTestDelegate() = default;

  void WillTearDown() {
    if (texture_id_) {
      glDeleteTextures(1, &texture_id_);
    }
  }

  bool SkipTest(GLDisplay* display) const {
    ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                                ->GetSurfaceFactoryOzone()
                                ->GetCurrentGLOzone();
    if (!gl_ozone || !gl_ozone->CanImportNativePixmap()) {
      LOG(WARNING) << "Skip test, ozone implementation can't import native "
                   << "pixmaps";
      return true;
    }
    return false;
  }

  scoped_refptr<gpu::GLImageNativePixmap> CreateSolidColorImage(
      const gfx::Size& size,
      const uint8_t color[4]) {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<gfx::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget,
                                            nullptr, size, format, usage);
    DCHECK(pixmap) << "Offending format: " << gfx::BufferFormatToString(format);
    if (usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE ||
        usage == gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE ||
        usage == gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE) {
      auto client_pixmap = client_native_pixmap_factory_->ImportFromHandle(
          pixmap->ExportHandle(), size, format, usage);
      bool mapped = client_pixmap->Map();
      EXPECT_TRUE(mapped);

      for (size_t plane = 0; plane < pixmap->GetNumberOfPlanes(); ++plane) {
        void* data = client_pixmap->GetMemoryAddress(plane);
        GLTestSupport::SetBufferDataToColor(
            size.width(), size.height(), pixmap->GetDmaBufPitch(plane), plane,
            pixmap->GetBufferFormat(), color, static_cast<uint8_t*>(data));
      }
      client_pixmap->Unmap();
    }

    // Create a dummy texture ID to bind - these tests don't actually care about
    // binding.
    if (!texture_id_) {
      glGenTextures(1, &texture_id_);
    }

    auto image = gpu::GLImageNativePixmap::CreateForTesting(
        size, format, std::move(pixmap), GetTextureTarget(), texture_id_);
    EXPECT_TRUE(image);
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
    if (format == gfx::BufferFormat::P010) {
      return 3;
    }
    return 0;
  }

 private:
  GLuint texture_id_ = 0;
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
};

using GLImageScanoutType = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_SUITE_P(GLImageNativePixmapScanoutBGRA,
                               GLImageTest,
                               GLImageScanoutType);

using GLImageScanoutTypeDisabled = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::RGBA_1010102>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::BGRA_1010102>>;

// This test is disabled since we need mesa support for AB30 that is not
// available on many boards yet.
INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_GLImageNativePixmapScanoutRGBA,
                               GLImageTest,
                               GLImageScanoutTypeDisabled);

}  // namespace
}  // namespace gl

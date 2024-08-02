// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringize_macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_support.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

// TODO(crbug.com/40630408): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_Create DISABLED_Create
#else
#define MAYBE_Create Create
#endif

namespace gl {
namespace {

constexpr gfx::BufferUsage kUsage = gfx::BufferUsage::SCANOUT;
constexpr gfx::BufferFormat kFormat = gfx::BufferFormat::BGRA_8888;

bool SkipTest() {
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);

  ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                              ->GetSurfaceFactoryOzone()
                              ->GetCurrentGLOzone();
  if (!gl_ozone || !gl_ozone->CanImportNativePixmap(kFormat)) {
    LOG(WARNING) << "Skip test, ozone implementation can't import native "
                 << "pixmaps";
    return true;
  }
  return false;
}

class NativePixmapGLBindingTest : public testing::Test {
 public:
  NativePixmapGLBindingTest() {
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  }

 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    if (SkipTest()) {
      GTEST_SKIP();
    }
    display_ = GLTestSupport::InitializeGL(std::nullopt);
    surface_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), GLContextAttribs());
    context_->MakeCurrent(surface_.get());
  }
  void TearDown() override {
    if (texture_id_) {
      glDeleteTextures(1, &texture_id_);
    }
    if (context_) {
      context_->ReleaseCurrent(surface_.get());
      context_ = nullptr;
    }
    surface_ = nullptr;
    GLTestSupport::CleanupGL(display_);
  }

 protected:
  std::unique_ptr<ui::NativePixmapGLBinding> CreateSolidColorImage(
      const gfx::Size& size) {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<gfx::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget,
                                            nullptr, size, kFormat, kUsage);
    DCHECK(pixmap) << "Offending format: "
                   << gfx::BufferFormatToString(kFormat);

    // Create a dummy texture ID to bind - these tests don't actually care about
    // binding.
    if (!texture_id_) {
      glGenTextures(1, &texture_id_);
    }

    ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                                ->GetSurfaceFactoryOzone()
                                ->GetCurrentGLOzone();
    EXPECT_TRUE(gl_ozone->CanImportNativePixmap(kFormat));

    auto binding = gl_ozone->ImportNativePixmap(
        std::move(pixmap), kFormat, gfx::BufferPlane::DEFAULT, size,
        gfx::ColorSpace(), GL_TEXTURE_EXTERNAL_OES, texture_id_);
    EXPECT_TRUE(binding);
    return binding;
  }

  scoped_refptr<GLSurface> surface_;
  scoped_refptr<GLContext> context_;
  GLuint texture_id_ = 0;
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(NativePixmapGLBindingTest, MAYBE_Create) {
  // NOTE: On some drm devices (mediatek) the minimum width/height to add an fb
  // for a bo must be 64, and YVU_420 in i915 requires at least 128 length.
  const gfx::Size small_image_size(128, 128);
  const gfx::Size large_image_size(512, 512);

  // Create a small solid color green image of preferred format.
  auto small_image = CreateSolidColorImage(small_image_size);
  ASSERT_TRUE(small_image);

  // Create a large solid color green image of preferred format.
  auto large_image = CreateSolidColorImage(large_image_size);
  ASSERT_TRUE(large_image);
}

}  // namespace
}  // namespace gl

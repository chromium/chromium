// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(OS_WIN)
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"
#endif
// TODO(crbug.com/969798): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_SurfaceFormatTest DISABLED_SurfaceFormatTest
#else
#define MAYBE_SurfaceFormatTest SurfaceFormatTest
#endif

namespace gl {

namespace {

class GLSurfaceEGLTest : public testing::Test {
 protected:
  void SetUp() override {
#if defined(OS_WIN)
    GLSurfaceTestSupport::InitializeOneOffImplementation(
        GLImplementation::kGLImplementationEGLANGLE, true);
#else
    GLSurfaceTestSupport::InitializeOneOffImplementation(
        GLImplementation::kGLImplementationEGLGLES2, true);
#endif
  }

  void TearDown() override { gl::init::ShutdownGL(false); }
};

#if !defined(MEMORY_SANITIZER)
// Fails under MSAN: crbug.com/886995
TEST_F(GLSurfaceEGLTest, MAYBE_SurfaceFormatTest) {
  GLSurfaceFormat surface_format = GLSurfaceFormat();
  surface_format.SetDepthBits(24);
  surface_format.SetStencilBits(8);
  surface_format.SetSamples(0);
  scoped_refptr<GLSurface> surface =
      init::CreateOffscreenGLSurfaceWithFormat(gfx::Size(1, 1), surface_format);
  EGLConfig config = surface->GetConfig();
  EXPECT_TRUE(config);

  EGLint attrib;
  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_DEPTH_SIZE, &attrib);
  EXPECT_LE(24, attrib);

  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_STENCIL_SIZE, &attrib);
  EXPECT_LE(8, attrib);

  eglGetConfigAttrib(surface->GetDisplay(), config, EGL_SAMPLES, &attrib);
  EXPECT_EQ(0, attrib);
}
#endif

#if defined(OS_WIN)

class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
};

TEST_F(GLSurfaceEGLTest, FixedSizeExtension) {
  TestPlatformDelegate platform_delegate;
  gfx::Size window_size(400, 500);
  ui::WinWindow window(&platform_delegate, gfx::Rect(window_size));

  scoped_refptr<GLSurface> surface = InitializeGLSurface(
      base::MakeRefCounted<NativeViewGLSurfaceEGL>(window.hwnd(), nullptr));
  ASSERT_TRUE(surface);
  EXPECT_EQ(window_size, surface->GetSize());

  scoped_refptr<GLContext> context = init::CreateGLContext(
      nullptr /* share_group */, surface.get(), GLContextAttribs());
  ASSERT_TRUE(context);
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  gfx::Size resize_size(200, 300);
  surface->Resize(resize_size, 1.0, GLSurface::ColorSpace::UNSPECIFIED, false);
  EXPECT_EQ(resize_size, surface->GetSize());
}

#endif
}  // namespace
}  // namespace gl

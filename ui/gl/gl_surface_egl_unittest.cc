// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_WIN)
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"
#endif

namespace gl {

namespace {

class GLSurfaceEGLTest : public testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
    display_ = GLSurfaceTestSupport::InitializeOneOffImplementation(
        GLImplementationParts(kGLImplementationEGLANGLE));
#else
    display_ = GLSurfaceTestSupport::InitializeOneOffImplementation(
        GLImplementationParts(kGLImplementationEGLGLES2));
#endif
  }

  void TearDown() override { GLSurfaceTestSupport::ShutdownGL(display_); }

  GLDisplay* GetTestDisplay() {
    EXPECT_NE(display_, nullptr);
    return display_;
  }

 private:
  raw_ptr<GLDisplay> display_ = nullptr;
};

#if BUILDFLAG(IS_WIN)

class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
};

TEST_F(GLSurfaceEGLTest, FixedSizeExtension) {
  TestPlatformDelegate platform_delegate;
  gfx::Size window_size(400, 500);
  ui::WinWindow window(&platform_delegate, gfx::Rect(window_size));

  scoped_refptr<GLSurface> surface =
      InitializeGLSurface(base::MakeRefCounted<NativeViewGLSurfaceEGL>(
          GLSurfaceEGL::GetGLDisplayEGL(), window.hwnd(), nullptr));
  ASSERT_TRUE(surface);
  EXPECT_EQ(window_size, surface->GetSize());

  scoped_refptr<GLContext> context = init::CreateGLContext(
      nullptr /* share_group */, surface.get(), GLContextAttribs());
  ASSERT_TRUE(context);
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  gfx::Size resize_size(200, 300);
  surface->Resize(resize_size, 1.0, gfx::ColorSpace(), false);
  EXPECT_EQ(resize_size, surface->GetSize());
}

#endif
}  // namespace
}  // namespace gl

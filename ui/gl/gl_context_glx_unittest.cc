// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_glx.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_glx_x11.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_image_test_support.h"

namespace gl {

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
// https://crbug.com/830653
#define MAYBE_DoNotDestroyOnFailedMakeCurrent \
  DISABLED_DoNotDestroyOnFailedMakeCurrent
#else
#define MAYBE_DoNotDestroyOnFailedMakeCurrent DoNotDestroyOnFailedMakeCurrent
#endif

TEST(GLContextGLXTest, MAYBE_DoNotDestroyOnFailedMakeCurrent) {
  auto* xdisplay = gfx::GetXDisplay();
  ASSERT_TRUE(xdisplay);

  gfx::X11ErrorTracker error_tracker;

  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = 0;
  swa.override_redirect = x11::True;
  auto xwindow = XCreateWindow(xdisplay, DefaultRootWindow(xdisplay), 0, 0, 10,
                               10,              // x, y, width, height
                               0,               // border width
                               CopyFromParent,  // depth
                               InputOutput,
                               CopyFromParent,  // visual
                               CWBackPixmap | CWOverrideRedirect, &swa);
  XSelectInput(xdisplay, xwindow, StructureNotifyMask);

  XEvent xevent;
  XMapWindow(xdisplay, xwindow);
  // Wait until the window is mapped.
  while (XNextEvent(xdisplay, &xevent) && xevent.type != MapNotify &&
         xevent.xmap.window != xwindow) {
  }

  GLImageTestSupport::InitializeGL(base::nullopt);
  auto surface =
      gl::InitializeGLSurface(base::MakeRefCounted<GLSurfaceGLXX11>(xwindow));
  scoped_refptr<GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), GLContextAttribs());

  // Verify that MakeCurrent() is successful.
  ASSERT_TRUE(context->GetHandle());
  ASSERT_TRUE(context->MakeCurrent(surface.get()));
  EXPECT_TRUE(context->GetHandle());

  // Destroy the window, and wait until the window is unmapped. There should be
  // no x11 errors.
  context->ReleaseCurrent(surface.get());
  XDestroyWindow(xdisplay, xwindow);
  while (XNextEvent(xdisplay, &xevent) && xevent.type != UnmapNotify) {
  }
  ASSERT_FALSE(error_tracker.FoundNewError());

  if (context->MakeCurrent(surface.get())) {
    // With some drivers, MakeCurrent() does not fail for an already-destroyed
    // window. In those cases, override the glx api to force MakeCurrent() to
    // fail.
    context->ReleaseCurrent(surface.get());
    auto real_fn = g_driver_glx.fn.glXMakeContextCurrentFn;
    g_driver_glx.fn.glXMakeContextCurrentFn =
        [](Display* display, GLXDrawable drawable, GLXDrawable read,
           GLXContext context) -> int { return 0; };
    EXPECT_FALSE(context->MakeCurrent(surface.get()));
    g_driver_glx.fn.glXMakeContextCurrentFn = real_fn;
  }
  // At this point, MakeCurrent() failed. Make sure the GLContextGLX still was
  // not destroyed.
  ASSERT_TRUE(context->GetHandle());
  surface = nullptr;
  XSync(xdisplay, x11::True);
}

}  // namespace gl

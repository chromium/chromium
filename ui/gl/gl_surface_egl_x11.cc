// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_x11.h"

#include "base/containers/contains.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/egl_util.h"

namespace gl {

NativeViewGLSurfaceEGLX11::NativeViewGLSurfaceEGLX11(GLDisplayEGL* display,
                                                     x11::Window window)
    : NativeViewGLSurfaceEGL(display, static_cast<uint32_t>(window), nullptr) {}

bool NativeViewGLSurfaceEGLX11::Initialize(GLSurfaceFormat format) {
  if (!NativeViewGLSurfaceEGL::Initialize(format))
    return false;

  auto* connection = x11::Connection::Get();
  // Query all child windows and store them. ANGLE creates a child window when
  // eglCreateWindowSurface is called on X11 and expose events from this window
  // need to be received by this class.  Since ANGLE is using a separate
  // connection, we have to select expose events on our own connection.
  if (auto reply =
          connection->QueryTree({static_cast<x11::Window>(window_)}).Sync()) {
    children_ = std::move(reply->children);
  }
  for (auto child : children_) {
    connection->ChangeWindowAttributes(
        {.window = child, .event_mask = x11::EventMask::Exposure});
  }

  dispatcher_set_ = true;
  connection->AddEventObserver(this);
  return true;
}

void NativeViewGLSurfaceEGLX11::Destroy() {
  NativeViewGLSurfaceEGL::Destroy();
  if (dispatcher_set_)
    x11::Connection::Get()->RemoveEventObserver(this);
}

gfx::SwapResult NativeViewGLSurfaceEGLX11::SwapBuffers(
    PresentationCallback callback,
    gfx::FrameData data) {
  auto result = NativeViewGLSurfaceEGL::SwapBuffers(std::move(callback), data);
  if (result == gfx::SwapResult::SWAP_FAILED)
    return result;

  // We need to restore the background pixel that we set to WhitePixel on
  // views::DesktopWindowTreeHostX11::InitX11Window back to None for the
  // XWindow associated to this surface after the first SwapBuffers has
  // happened, to avoid showing a weird white background while resizing.
  if (GetXNativeConnection()->Ready() && !has_swapped_buffers_) {
    GetXNativeConnection()->ChangeWindowAttributes({
        .window = static_cast<x11::Window>(window_),
        .background_pixmap = x11::Pixmap::None,
    });
    GetXNativeConnection()->Flush();
    has_swapped_buffers_ = true;
  }
  return result;
}

EGLint NativeViewGLSurfaceEGLX11::GetNativeVisualID() const {
  x11::VisualId visual_id;
  GetXNativeConnection()->GetOrCreateVisualManager().ChooseVisualForWindow(
      true, &visual_id, nullptr, nullptr, nullptr);
  return static_cast<EGLint>(visual_id);
}

NativeViewGLSurfaceEGLX11::~NativeViewGLSurfaceEGLX11() {
  InvalidateWeakPtrs();
  Destroy();
}

x11::Connection* NativeViewGLSurfaceEGLX11::GetXNativeConnection() const {
  return x11::Connection::Get();
}

std::unique_ptr<gfx::VSyncProvider>
NativeViewGLSurfaceEGLX11::CreateVsyncProviderInternal() {
  return std::make_unique<ui::XrandrIntervalOnlyVSyncProvider>();
}

void NativeViewGLSurfaceEGLX11::OnEvent(const x11::Event& x11_event) {
  // When ANGLE is used for EGL, it creates an X11 child window. Expose events
  // from this window need to be forwarded to this class.
  auto* expose = x11_event.As<x11::ExposeEvent>();
  if (!expose || !base::Contains(children_, expose->window))
    return;

  auto expose_copy = *expose;
  auto window = static_cast<x11::Window>(window_);
  expose_copy.window = window;
  x11::Connection::Get()->SendEvent(expose_copy, window,
                                    x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
}

}  // namespace gl

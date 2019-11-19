// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  GLOzoneEGLWayland(WaylandConnection* connection,
                    WaylandBufferManagerGpu* buffer_manager)
      : connection_(connection), buffer_manager_(buffer_manager) {}
  ~GLOzoneEGLWayland() override {}

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;

 protected:
  intptr_t GetNativeDisplay() override;
  bool LoadGLES2Bindings(gl::GLImplementation impl) override;

 private:
  WaylandConnection* const connection_;
  WaylandBufferManagerGpu* const buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLWayland);
};

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
  // Only EGLGLES2 is supported with surfaceless view gl.
  if ((gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2) ||
      !connection_)
    return nullptr;

  WaylandWindow* window =
      connection_->wayland_window_manager()->GetWindow(widget);
  if (!window)
    return nullptr;

  // The wl_egl_window needs to be created before the GLSurface so it can be
  // used in the GLSurface constructor.
  auto egl_window = CreateWaylandEglWindow(window);
  if (!egl_window)
    return nullptr;
  return gl::InitializeGLSurface(new GLSurfaceWayland(std::move(egl_window)));
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  // Only EGLGLES2 is supported with surfaceless view gl.
  if (gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2)
    return nullptr;

#if defined(WAYLAND_GBM)
  // If there is a gbm device available, use surfaceless gl surface.
  if (!buffer_manager_->gbm_device())
    return nullptr;
  return gl::InitializeGLSurface(
      new GbmSurfacelessWayland(buffer_manager_, window));
#else
  return nullptr;
#endif
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  if (gl::GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
      size.width() == 0 && size.height() == 0) {
    return gl::InitializeGLSurface(new gl::SurfacelessEGL(size));
  } else {
    return gl::InitializeGLSurface(new gl::PbufferGLSurfaceEGL(size));
  }
}

intptr_t GLOzoneEGLWayland::GetNativeDisplay() {
  if (connection_)
    return reinterpret_cast<intptr_t>(connection_->display());
  return EGL_DEFAULT_DISPLAY;
}

bool GLOzoneEGLWayland::LoadGLES2Bindings(gl::GLImplementation impl) {
  // TODO: It may not be necessary to set this environment variable when using
  // swiftshader.
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(impl);
}

}  // namespace

WaylandSurfaceFactory::WaylandSurfaceFactory(
    WaylandConnection* connection,
    WaylandBufferManagerGpu* buffer_manager)
    : connection_(connection), buffer_manager_(buffer_manager) {
  egl_implementation_ =
      std::make_unique<GLOzoneEGLWayland>(connection_, buffer_manager_);
}

WaylandSurfaceFactory::~WaylandSurfaceFactory() = default;

std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget,
                                             base::TaskRunner* task_runner) {
  return std::make_unique<WaylandCanvasSurface>(buffer_manager_, widget);
}

std::vector<gl::GLImplementation>
WaylandSurfaceFactory::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementation> impls;
  if (egl_implementation_) {
    impls.push_back(gl::kGLImplementationEGLGLES2);
    impls.push_back(gl::kGLImplementationSwiftShaderGL);
  }
  return impls;
}

GLOzone* WaylandSurfaceFactory::GetGLOzone(
    gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationSwiftShaderGL:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

scoped_refptr<gfx::NativePixmap> WaylandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if defined(WAYLAND_GBM)
  scoped_refptr<GbmPixmapWayland> pixmap =
      base::MakeRefCounted<GbmPixmapWayland>(buffer_manager_);

  if (widget != gfx::kNullAcceleratedWidget)
    pixmap->SetAcceleratedWiget(widget);

  if (!pixmap->InitializeBuffer(size, format, usage))
    return nullptr;
  return pixmap;
#else
  return nullptr;
#endif
}

void WaylandSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  // CreateNativePixmap is non-blocking operation. Thus, it is safe to call it
  // and return the result with the provided callback.
  std::move(callback).Run(
      CreateNativePixmap(widget, vk_device, size, format, usage));
}

scoped_refptr<gfx::NativePixmap>
WaylandSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui

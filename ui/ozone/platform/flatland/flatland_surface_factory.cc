// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface_factory.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/event.h>
#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/angle/src/common/fuchsia_egl/fuchsia_egl.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/flatland/flatland_gpu_service.h"
#include "ui/ozone/platform/flatland/flatland_surface.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_window.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/flatland/vulkan_implementation_flatland.h"
#endif

namespace ui {

namespace {

struct FuchsiaEGLWindowDeleter {
  void operator()(fuchsia_egl_window* egl_window) {
    fuchsia_egl_window_destroy(egl_window);
  }
};

class GLSurfaceFuchsiaImagePipe : public gl::NativeViewGLSurfaceEGL {
 public:
  explicit GLSurfaceFuchsiaImagePipe(
      FlatlandSurfaceFactory* flatland_surface_factory,
      gfx::AcceleratedWidget widget)
      : NativeViewGLSurfaceEGL(0, nullptr),
        flatland_surface_factory_(flatland_surface_factory),
        widget_(widget) {
    CHECK(widget_);
    CHECK(flatland_surface_factory_);
  }
  GLSurfaceFuchsiaImagePipe(const GLSurfaceFuchsiaImagePipe&) = delete;
  GLSurfaceFuchsiaImagePipe& operator=(const GLSurfaceFuchsiaImagePipe&) =
      delete;

  // gl::NativeViewGLSurfaceEGL:
  bool InitializeNativeWindow() override {
    // TODO(crbug.com/1230150): Create EGLNativeWindowType that consumes channel
    // required by the upcoming flatland vulkan extension.
    NOTREACHED();
    return false;
  }

  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override {
    fuchsia_egl_window_resize(egl_window_.get(), size.width(), size.height());
    return gl::NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                              has_alpha);
  }

 private:
  ~GLSurfaceFuchsiaImagePipe() override {}

  FlatlandSurfaceFactory* const flatland_surface_factory_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  std::unique_ptr<fuchsia_egl_window, FuchsiaEGLWindowDeleter> egl_window_;
};

class GLOzoneEGLFlatland : public GLOzoneEGL {
 public:
  explicit GLOzoneEGLFlatland(FlatlandSurfaceFactory* flatland_surface_factory)
      : flatland_surface_factory_(flatland_surface_factory) {}
  ~GLOzoneEGLFlatland() override = default;
  GLOzoneEGLFlatland(const GLOzoneEGLFlatland&) = delete;
  GLOzoneEGLFlatland& operator=(const GLOzoneEGLFlatland&) = delete;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<GLSurfaceFuchsiaImagePipe>(
            flatland_surface_factory_, window));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::SurfacelessEGL>(size));
  }

  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  }

 protected:
  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  FlatlandSurfaceFactory* const flatland_surface_factory_;
};

fuchsia::sysmem::AllocatorHandle ConnectSysmemAllocator() {
  fuchsia::sysmem::AllocatorHandle allocator;
  base::ComponentContextForProcess()->svc()->Connect(allocator.NewRequest());
  return allocator;
}

}  // namespace

FlatlandSurfaceFactory::FlatlandSurfaceFactory()
    : egl_implementation_(std::make_unique<GLOzoneEGLFlatland>(this)),
      flatland_sysmem_buffer_manager_(this),
      weak_ptr_factory_(this) {}

FlatlandSurfaceFactory::~FlatlandSurfaceFactory() {
  Shutdown();
}

void FlatlandSurfaceFactory::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());

  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(main_thread_task_runner_);

  DCHECK(!gpu_host_);
  gpu_host_.Bind(std::move(gpu_host));

  flatland_sysmem_buffer_manager_.Initialize(ConnectSysmemAllocator());
}

void FlatlandSurfaceFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());
  main_thread_task_runner_ = nullptr;
  gpu_host_.reset();
  flatland_sysmem_buffer_manager_.Shutdown();
}

std::vector<gl::GLImplementationParts>
FlatlandSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
      gl::GLImplementationParts(gl::kGLImplementationSwiftShaderGL),
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
      gl::GLImplementationParts(gl::kGLImplementationStubGL),
  };
}

GLOzone* FlatlandSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationSwiftShaderGL:
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<PlatformWindowSurface>
FlatlandSurfaceFactory::CreatePlatformWindowSurface(
    gfx::AcceleratedWidget window) {
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);
  auto surface = std::make_unique<FlatlandSurface>(this, window);
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FlatlandSurfaceFactory::AttachSurfaceToWindow,
                                weak_ptr_factory_.GetWeakPtr(), window,
                                surface->CreateView()));
  return surface;
}

std::unique_ptr<SurfaceOzoneCanvas>
FlatlandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  // TODO(crbug.com/1230150): Add FlatlandWindowCanvas implementation.
  NOTREACHED();
  return nullptr;
}

scoped_refptr<gfx::NativePixmap> FlatlandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  DCHECK(!framebuffer_size || framebuffer_size == size);
  auto collection = flatland_sysmem_buffer_manager_.CreateCollection(
      vk_device, size, format, usage, 1);
  if (!collection)
    return nullptr;

  return collection->CreateNativePixmap(0);
}

void FlatlandSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  std::move(callback).Run(
      CreateNativePixmap(widget, vk_device, size, format, usage));
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
FlatlandSurfaceFactory::CreateVulkanImplementation(
    bool use_swiftshader,
    bool allow_protected_memory) {
  return std::make_unique<ui::VulkanImplementationFlatland>(
      this, &flatland_sysmem_buffer_manager_, allow_protected_memory);
}
#endif

void FlatlandSurfaceFactory::AddSurface(gfx::AcceleratedWidget widget,
                                        FlatlandSurface* surface) {
  base::AutoLock lock(surface_lock_);
  DCHECK(!base::Contains(surface_map_, widget));
  surface->AssertBelongsToCurrentThread();
  surface_map_.emplace(widget, surface);
}

void FlatlandSurfaceFactory::RemoveSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  FlatlandSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  surface_map_.erase(it);
}

FlatlandSurface* FlatlandSurfaceFactory::GetSurface(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  if (it == surface_map_.end())
    return nullptr;

  FlatlandSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  return surface;
}

void FlatlandSurfaceFactory::AttachSurfaceToWindow(
    gfx::AcceleratedWidget window,
    mojo::PlatformHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_host_->AttachSurfaceToWindow(window,
                                   std::move(surface_view_holder_token_mojo));
}

}  // namespace ui

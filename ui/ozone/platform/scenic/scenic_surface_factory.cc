// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/event.h>
#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/memory/ptr_util.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/angle/src/common/fuchsia_egl/fuchsia_egl.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/scenic/scenic_gpu_service.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_canvas.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"
#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"

namespace ui {

namespace {

fuchsia::ui::scenic::ScenicPtr ConnectToScenic() {
  fuchsia::ui::scenic::ScenicPtr scenic =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::ui::scenic::Scenic>();
  scenic.set_error_handler(
      base::LogFidlErrorAndExitProcess(FROM_HERE, "fuchsia.ui.scenic.Scenic"));
  return scenic;
}

class GLOzoneEGLScenic : public GLOzoneEGL {
 public:
  GLOzoneEGLScenic() {}

  GLOzoneEGLScenic(const GLOzoneEGLScenic&) = delete;
  GLOzoneEGLScenic& operator=(const GLOzoneEGLScenic&) = delete;

  ~GLOzoneEGLScenic() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    // GL rendering to Flatland views is not supported. This function is
    // used only for unittests. Return an off-screen surface, so the tests pass.
    // TODO(crbug.com/1271760): Use Vulkan in unittests and remove this hack.
    return gl::InitializeGLSurface(base::MakeRefCounted<gl::SurfacelessEGL>(
        display->GetAs<gl::GLDisplayEGL>(), gfx::Size(100, 100)));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(base::MakeRefCounted<gl::SurfacelessEGL>(
        display->GetAs<gl::GLDisplayEGL>(), size));
  }

  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  }

 protected:
  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }
};

fuchsia::sysmem::AllocatorHandle ConnectSysmemAllocator() {
  fuchsia::sysmem::AllocatorHandle allocator;
  base::ComponentContextForProcess()->svc()->Connect(allocator.NewRequest());
  return allocator;
}

}  // namespace

ScenicSurfaceFactory::ScenicSurfaceFactory()
    : egl_implementation_(std::make_unique<GLOzoneEGLScenic>()),
      sysmem_buffer_manager_(this),
      weak_ptr_factory_(this) {}

ScenicSurfaceFactory::~ScenicSurfaceFactory() {
  Shutdown();
}

void ScenicSurfaceFactory::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());

  main_thread_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  DCHECK(main_thread_task_runner_);

  DCHECK(!gpu_host_);
  gpu_host_.Bind(std::move(gpu_host));

  sysmem_buffer_manager_.Initialize(ConnectSysmemAllocator());

  // Scenic is lazily connected to avoid a dependency in headless mode.
  DCHECK(!scenic_);
}

void ScenicSurfaceFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());
  main_thread_task_runner_ = nullptr;
  gpu_host_.reset();
  sysmem_buffer_manager_.Shutdown();
  scenic_ = nullptr;
}

std::vector<gl::GLImplementationParts>
ScenicSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
      gl::GLImplementationParts(gl::ANGLEImplementation::kSwiftShader),
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
      gl::GLImplementationParts(gl::kGLImplementationStubGL),
  };
}

GLOzone* ScenicSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<PlatformWindowSurface>
ScenicSurfaceFactory::CreatePlatformWindowSurface(
    gfx::AcceleratedWidget window) {
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);
  auto surface = std::make_unique<ScenicSurface>(this, &sysmem_buffer_manager_,
                                                 window, CreateScenicSession());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScenicSurfaceFactory::AttachSurfaceToWindow,
                                weak_ptr_factory_.GetWeakPtr(), window,
                                surface->CreateView()));
  return surface;
}

std::unique_ptr<SurfaceOzoneCanvas> ScenicSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  ScenicSurface* surface = GetSurface(widget);
  return std::make_unique<ScenicWindowCanvas>(surface);
}

scoped_refptr<gfx::NativePixmap> ScenicSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  DCHECK(!framebuffer_size || framebuffer_size == size);

  VkDevice vk_device = device_queue->GetVulkanDevice();
  if (widget != gfx::kNullAcceleratedWidget &&
      usage == gfx::BufferUsage::SCANOUT) {
    // The usage SCANOUT is for a primary plane buffer.
    auto* surface = GetSurface(widget);
    CHECK(surface);
    return surface->AllocatePrimaryPlanePixmap(vk_device, size, format);
  }

  return sysmem_buffer_manager_.CreateNativePixmap(vk_device, size, format,
                                                   usage);
}

void ScenicSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  std::move(callback).Run(
      CreateNativePixmap(widget, device_queue, size, format, usage));
}

scoped_refptr<gfx::NativePixmap>
ScenicSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  auto collection = sysmem_buffer_manager_.GetCollectionByHandle(
      handle.buffer_collection_handle);
  if (!collection)
    return nullptr;

  return collection->CreateNativePixmap(std::move(handle), size);
}

std::unique_ptr<gpu::VulkanImplementation>
ScenicSurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                                 bool allow_protected_memory) {
  return std::make_unique<ui::VulkanImplementationScenic>(
      this, &sysmem_buffer_manager_, use_swiftshader, allow_protected_memory);
}

std::vector<gfx::BufferFormat>
ScenicSurfaceFactory::GetSupportedFormatsForTexturing() const {
  return {
      gfx::BufferFormat::R_8,
      gfx::BufferFormat::RG_88,
      gfx::BufferFormat::RGBA_8888,
      gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::BGRA_8888,
      gfx::BufferFormat::BGRX_8888,
      gfx::BufferFormat::YUV_420_BIPLANAR,
  };
}

void ScenicSurfaceFactory::AddSurface(gfx::AcceleratedWidget widget,
                                      ScenicSurface* surface) {
  base::AutoLock lock(surface_lock_);
  DCHECK(!base::Contains(surface_map_, widget));
  surface->AssertBelongsToCurrentThread();
  surface_map_.emplace(widget, surface);
}

void ScenicSurfaceFactory::RemoveSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  surface_map_.erase(it);
}

ScenicSurface* ScenicSurfaceFactory::GetSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  if (it == surface_map_.end())
    return nullptr;

  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  return surface;
}

scenic::SessionPtrAndListenerRequest
ScenicSurfaceFactory::CreateScenicSession() {
  fuchsia::ui::scenic::SessionPtr session;
  fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener_handle;
  auto listener_request = listener_handle.NewRequest();

  {
    // Cache Scenic connection for main thread. For other treads create
    // one-shot connection.
    fuchsia::ui::scenic::ScenicPtr local_scenic;
    fuchsia::ui::scenic::ScenicPtr* scenic =
        main_thread_task_runner_->BelongsToCurrentThread() ? &scenic_
                                                           : &local_scenic;
    if (!*scenic)
      *scenic = ConnectToScenic();
    (*scenic)->CreateSession(session.NewRequest(), std::move(listener_handle));
  }

  return {std::move(session), std::move(listener_request)};
}

void ScenicSurfaceFactory::AttachSurfaceToWindow(
    gfx::AcceleratedWidget window,
    mojo::PlatformHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_host_->AttachSurfaceToWindow(window,
                                   std::move(surface_view_holder_token_mojo));
}

}  // namespace ui

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <gbm.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_device.h"
#include "ui/ozone/common/linux/gbm_wrapper.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

uint32_t BufferUsageToGbmFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT:
      return GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
      break;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_SCANOUT |
             GBM_BO_USE_TEXTURING;
      break;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_DECODER;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;
  }
}

void CreateBufferWithGbmFlags(const scoped_refptr<DrmDevice>& drm,
                              uint32_t fourcc_format,
                              const gfx::Size& size,
                              uint32_t flags,
                              const std::vector<uint64_t>& modifiers,
                              std::unique_ptr<GbmBuffer>* out_buffer,
                              scoped_refptr<DrmFramebuffer>* out_framebuffer) {
  std::unique_ptr<GbmBuffer> buffer =
      drm->gbm_device()->CreateBufferWithModifiers(fourcc_format, size, flags,
                                                   modifiers);
  if (!buffer)
    return;

  scoped_refptr<DrmFramebuffer> framebuffer;
  if (flags & GBM_BO_USE_SCANOUT) {
    framebuffer = DrmFramebuffer::AddFramebuffer(drm, buffer.get());
    if (!framebuffer)
      return;
  }

  *out_buffer = std::move(buffer);
  *out_framebuffer = std::move(framebuffer);
}

class GbmDeviceGenerator : public DrmDeviceGenerator {
 public:
  GbmDeviceGenerator() {}
  ~GbmDeviceGenerator() override {}

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file,
                                        bool is_primary_device) override {
    auto gbm = CreateGbmDevice(file.GetPlatformFile());
    if (!gbm) {
      PLOG(ERROR) << "Unable to initialize GBM for " << path.value();
      return nullptr;
    }

    auto drm = base::MakeRefCounted<DrmDevice>(
        path, std::move(file), is_primary_device, std::move(gbm));
    if (!drm->Initialize())
      return nullptr;
    return drm;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(GbmDeviceGenerator);
};

}  // namespace

DrmThread::DrmThread() : base::Thread("DrmThread"), weak_ptr_factory_(this) {}

DrmThread::~DrmThread() {
  Stop();
}

void DrmThread::Start(base::OnceClosure binding_completer) {
  complete_early_binding_requests_ = std::move(binding_completer);
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_options.priority = base::ThreadPriority::DISPLAY;

  if (!StartWithOptions(thread_options))
    LOG(FATAL) << "Failed to create DRM thread";
}

void DrmThread::Init() {
  device_manager_.reset(
      new DrmDeviceManager(std::make_unique<GbmDeviceGenerator>()));
  screen_manager_.reset(new ScreenManager());

  display_manager_.reset(
      new DrmGpuDisplayManager(screen_manager_.get(), device_manager_.get()));

  DCHECK(task_runner())
      << "DrmThread::Init -- thread doesn't have a task_runner";

  // DRM thread is running now so can safely handle binding requests. So drain
  // the queue of as-yet unhandled binding requests if there are any.
  std::move(complete_early_binding_requests_).Run();
}

void DrmThread::CreateBuffer(gfx::AcceleratedWidget widget,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             uint32_t client_flags,
                             std::unique_ptr<GbmBuffer>* buffer,
                             scoped_refptr<DrmFramebuffer>* framebuffer) {
  scoped_refptr<ui::DrmDevice> drm = device_manager_->GetDrmDevice(widget);
  CHECK(drm) << "No devices available for buffer allocation.";

  DrmWindow* window = screen_manager_->GetWindow(widget);
  uint32_t flags = BufferUsageToGbmFlags(usage);
  uint32_t fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);

  // TODO(hoegsberg): We shouldn't really get here without a window,
  // but it happens during init. Need to figure out why.
  std::vector<uint64_t> modifiers;
  if (window && window->GetController() && !(flags & GBM_BO_USE_LINEAR) &&
      !(client_flags & GbmPixmap::kFlagNoModifiers)) {
    modifiers = window->GetController()->GetFormatModifiers(fourcc_format);
  }

  CreateBufferWithGbmFlags(drm, fourcc_format, size, flags, modifiers, buffer,
                           framebuffer);

  // NOTE: BufferUsage::SCANOUT is used to create buffers that will be
  // explicitly set via kms on a CRTC (e.g: BufferQueue buffers), therefore
  // allocation should fail if it's not possible to allocate a BO_USE_SCANOUT
  // buffer in that case.
  if (!*buffer && usage != gfx::BufferUsage::SCANOUT) {
    flags &= ~GBM_BO_USE_SCANOUT;
    CreateBufferWithGbmFlags(drm, fourcc_format, size, flags, modifiers, buffer,
                             framebuffer);
  }
}

void DrmThread::CreateBufferFromFds(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::vector<base::ScopedFD> fds,
    const std::vector<gfx::NativePixmapPlane>& planes,
    std::unique_ptr<GbmBuffer>* out_buffer,
    scoped_refptr<DrmFramebuffer>* out_framebuffer) {
  scoped_refptr<ui::DrmDevice> drm = device_manager_->GetDrmDevice(widget);
  DCHECK(drm);

  std::unique_ptr<GbmBuffer> buffer = drm->gbm_device()->CreateBufferFromFds(
      ui::GetFourCCFormatFromBufferFormat(format), size, std::move(fds),
      planes);
  if (!buffer)
    return;

  scoped_refptr<DrmFramebuffer> framebuffer;
  if (buffer->GetFlags() & GBM_BO_USE_SCANOUT) {
    // NB: This is not required to succeed; framebuffers are added for
    // imported buffers on a best effort basis.
    framebuffer = DrmFramebuffer::AddFramebuffer(drm, buffer.get());
  }

  *out_buffer = std::move(buffer);
  *out_framebuffer = std::move(framebuffer);
}

void DrmThread::GetScanoutFormats(
    gfx::AcceleratedWidget widget,
    std::vector<gfx::BufferFormat>* scanout_formats) {
  display_manager_->GetScanoutFormats(widget, scanout_formats);
}

void DrmThread::SchedulePageFlip(
    gfx::AcceleratedWidget widget,
    std::vector<DrmOverlayPlane> planes,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  scoped_refptr<ui::DrmDevice> drm_device =
      device_manager_->GetDrmDevice(widget);

  drm_device->plane_manager()->RequestPlanesReadyCallback(
      std::move(planes), base::BindOnce(&DrmThread::OnPlanesReadyForPageFlip,
                                        weak_ptr_factory_.GetWeakPtr(), widget,
                                        std::move(submission_callback),
                                        std::move(presentation_callback)));
}

void DrmThread::OnPlanesReadyForPageFlip(
    gfx::AcceleratedWidget widget,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback,
    std::vector<DrmOverlayPlane> planes) {
  DrmWindow* window = screen_manager_->GetWindow(widget);
  if (window) {
    window->SchedulePageFlip(std::move(planes), std::move(submission_callback),
                             std::move(presentation_callback));
  } else {
    std::move(submission_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
  }
}

void DrmThread::IsDeviceAtomic(gfx::AcceleratedWidget widget, bool* is_atomic) {
  scoped_refptr<ui::DrmDevice> drm_device =
      device_manager_->GetDrmDevice(widget);

  *is_atomic = drm_device && drm_device->is_atomic();
}

void DrmThread::CreateWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window(
      new DrmWindow(widget, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  screen_manager_->AddWindow(widget, std::move(window));
}

void DrmThread::DestroyWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window = screen_manager_->RemoveWindow(widget);
  window->Shutdown();
}

void DrmThread::SetWindowBounds(gfx::AcceleratedWidget widget,
                                const gfx::Rect& bounds) {
  screen_manager_->GetWindow(widget)->SetBounds(bounds);
}

void DrmThread::SetCursor(gfx::AcceleratedWidget widget,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& location,
                          int32_t frame_delay_ms) {
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmThread::MoveCursor(gfx::AcceleratedWidget widget,
                           const gfx::Point& location) {
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmThread::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& overlays,
    base::OnceCallback<void(gfx::AcceleratedWidget,
                            const OverlaySurfaceCandidateList&,
                            const OverlayStatusList&)> callback) {
  TRACE_EVENT0("drm,hwoverlays", "DrmThread::CheckOverlayCapabilities");

  auto params = CreateParamsFromOverlaySurfaceCandidate(overlays);
  std::move(callback).Run(
      widget, overlays,
      CreateOverlayStatusListFrom(
          screen_manager_->GetWindow(widget)->TestPageFlip(params)));
}

void DrmThread::RefreshNativeDisplays(
    base::OnceCallback<void(MovableDisplaySnapshots)> callback) {
  std::move(callback).Run(display_manager_->GetDisplays());
}

void DrmThread::ConfigureNativeDisplay(
    int64_t id,
    std::unique_ptr<display::DisplayMode> mode,
    const gfx::Point& origin,
    base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(
      id, display_manager_->ConfigureDisplay(id, *mode, origin));
}

void DrmThread::DisableNativeDisplay(
    int64_t id,
    base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(id, display_manager_->DisableDisplay(id));
}

void DrmThread::TakeDisplayControl(base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(display_manager_->TakeDisplayControl());
}

void DrmThread::RelinquishDisplayControl(
    base::OnceCallback<void(bool)> callback) {
  display_manager_->RelinquishDisplayControl();
  std::move(callback).Run(true);
}

void DrmThread::AddGraphicsDevice(const base::FilePath& path, base::File file) {
  device_manager_->AddDrmDevice(path, std::move(file));
}

void DrmThread::RemoveGraphicsDevice(const base::FilePath& path) {
  device_manager_->RemoveDrmDevice(path);
}

void DrmThread::GetHDCPState(
    int64_t display_id,
    base::OnceCallback<void(int64_t, bool, display::HDCPState)> callback) {
  display::HDCPState state = display::HDCP_STATE_UNDESIRED;
  bool success = display_manager_->GetHDCPState(display_id, &state);
  std::move(callback).Run(display_id, success, state);
}

void DrmThread::SetHDCPState(int64_t display_id,
                             display::HDCPState state,
                             base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(display_id,
                          display_manager_->SetHDCPState(display_id, state));
}

void DrmThread::SetColorMatrix(int64_t display_id,
                               const std::vector<float>& color_matrix) {
  display_manager_->SetColorMatrix(display_id, color_matrix);
}

void DrmThread::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  display_manager_->SetGammaCorrection(display_id, degamma_lut, gamma_lut);
}

void DrmThread::StartDrmDevice(StartDrmDeviceCallback callback) {
  // We currently assume that |Init| always succeeds so return true to indicate
  // when the DRM thread has completed launching.  In particular, the invocation
  // of the callback in the client triggers the invocation of DRM thread
  // readiness observers.
  std::move(callback).Run(true);
}

// DrmThread requires a BindingSet instead of a simple Binding because it will
// be used from multiple threads in multiple processes.
void DrmThread::AddBindingCursorDevice(
    ozone::mojom::DeviceCursorRequest request) {
  cursor_bindings_.AddBinding(this, std::move(request));
}

void DrmThread::AddBindingDrmDevice(ozone::mojom::DrmDeviceRequest request) {
  TRACE_EVENT0("drm", "DrmThread::AddBindingDrmDevice");
  drm_bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui

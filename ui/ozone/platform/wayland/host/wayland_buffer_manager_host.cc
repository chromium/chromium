// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <presentation-time-client-protocol.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/dmabuf_uapi.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_dmabuf.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_single_pixel.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_solid_color.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

namespace {

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

struct KernelVersion {
  int32_t major;
  int32_t minor;
  int32_t bugfix;
};

KernelVersion KernelVersionNumbers() {
  KernelVersion ver;
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED_IN_MIGRATION();
    ver.major = 0;
    ver.minor = 0;
    ver.bugfix = 0;
    return ver;
  }
  int num_read =
      sscanf(info.release, "%d.%d.%d", &ver.major, &ver.minor, &ver.bugfix);
  if (num_read < 1) {
    ver.major = 0;
  }
  if (num_read < 2) {
    ver.minor = 0;
  }
  if (num_read < 3) {
    ver.bugfix = 0;
  }
  return ver;
}

bool CheckImportExportFence() {
  KernelVersion ver = KernelVersionNumbers();

  // DMA_BUF_IOCTL_{IMPORT,EXPORT}_SYNC_FILE was added in 6.0
  return ver.major >= 6;
}

}  // namespace

WaylandBufferManagerHost::WaylandBufferManagerHost(
    WaylandConnection* connection)
    : connection_(connection), receiver_(this) {}

WaylandBufferManagerHost::~WaylandBufferManagerHost() = default;

void WaylandBufferManagerHost::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
WaylandBufferManagerHost::BindInterface() {
  // Allow to rebind the interface if it hasn't been destroyed yet.
  if (receiver_.is_bound())
    OnChannelDestroyed();

  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
      buffer_manager_host;
  receiver_.Bind(buffer_manager_host.InitWithNewPipeAndPassReceiver());
  return buffer_manager_host;
}

void WaylandBufferManagerHost::OnChannelDestroyed() {
  DCHECK(base::CurrentUIThread::IsSet());

  buffer_backings_.clear();
  dma_buffers_.clear();
  for (auto* window : connection_->window_manager()->GetAllWindows())
    window->OnChannelDestroyed();

  buffer_manager_gpu_associated_.reset();
  receiver_.reset();
}

void WaylandBufferManagerHost::OnCommitOverlayError(
    const std::string& message) {
  error_message_ = message;
  TerminateGpuProcess();
}

base::Version WaylandBufferManagerHost::GetServerVersion() const {
  return connection_->GetServerVersion();
}

wl::BufferFormatsWithModifiersMap
WaylandBufferManagerHost::GetSupportedBufferFormats() const {
  return connection_->buffer_factory()->GetSupportedBufferFormats();
}

bool WaylandBufferManagerHost::SupportsDmabuf() const {
  return connection_->buffer_factory()->SupportsDmabuf();
}

bool WaylandBufferManagerHost::SupportsAcquireFence() const {
  return !!connection_->linux_explicit_synchronization_v1() ||
         connection_->UseImplicitSyncInterop();
}

bool WaylandBufferManagerHost::SupportsViewporter() const {
  return !!connection_->viewporter();
}

bool WaylandBufferManagerHost::SupportsNonBackedSolidColorBuffers() const {
  return !!connection_->surface_augmenter();
}

bool WaylandBufferManagerHost::SupportsOverlays() const {
  return connection_->ShouldUseOverlayDelegation();
}

bool WaylandBufferManagerHost::SupportsSinglePixelBuffer() const {
  return !!connection_->single_pixel_buffer();
}

uint32_t WaylandBufferManagerHost::GetSurfaceAugmentorVersion() const {
  auto* augmenter = connection_->surface_augmenter();
  return augmenter ? augmenter->GetSurfaceAugmentorVersion() : 0u;
}

void WaylandBufferManagerHost::SetWaylandBufferManagerGpu(
    mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
        buffer_manager_gpu_associated) {
  buffer_manager_gpu_associated_.Bind(std::move(buffer_manager_gpu_associated));
}

void WaylandBufferManagerHost::CreateDmabufBasedBuffer(
    mojo::PlatformHandle dmabuf_fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT2("wayland", "WaylandBufferManagerHost::CreateDmabufBasedBuffer",
               "Format", format, "Buffer id", buffer_id);

  base::ScopedFD fd = dmabuf_fd.TakeFD();

  // Validate data and ask surface to create a buffer associated with the
  // |buffer_id|.
  if (!ValidateDataFromGpu(fd, size, strides, offsets, modifiers, format,
                           planes_count, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  if (connection_->UseImplicitSyncInterop()) {
    dma_buffers_.emplace(buffer_id, dup(fd.get()));
  }

  // Check if any of the surfaces has already had a buffer with the same id.
  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingDmabuf>(
                     connection_, std::move(fd), size, std::move(strides),
                     std::move(offsets), std::move(modifiers), format,
                     planes_count, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

void WaylandBufferManagerHost::CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                                                    uint64_t length,
                                                    const gfx::Size& size,
                                                    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateShmBasedBuffer",
               "Buffer id", buffer_id);

  base::ScopedFD fd = shm_fd.TakeFD();
  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(fd, length, size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Check if any of the surfaces has already had a buffer with the same id.
  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingShm>(
                     connection_, std::move(fd), length, size, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

void WaylandBufferManagerHost::CreateSolidColorBuffer(const gfx::Size& size,
                                                      const SkColor4f& color,
                                                      uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());
  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateSolidColorBuffer",
               "Buffer id", buffer_id);

  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // OzonePlatform::PlatformInitProperties has a control variable that tells
  // viz to create a backing solid color buffers if the protocol is not
  // available. But in order to avoid a missusage of that variable and this
  // method (malformed requests), explicitly terminate the gpu.
  if (!connection_->surface_augmenter()) {
    error_message_ = "Surface augmenter protocol is not available.";
    TerminateGpuProcess();
    return;
  }

  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingSolidColor>(
                     connection_, color, size, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

void WaylandBufferManagerHost::CreateSinglePixelBuffer(const SkColor4f& color,
                                                       uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());
  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateSinglePixelBuffer",
               "Buffer id", buffer_id);

  const gfx::Size size = gfx::Size(1, 1);

  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // OzonePlatform::PlatformInitProperties has a control variable that tells
  // viz to create a backing single pixel buffers if the protocol is not
  // available. But in order to avoid a missusage of that variable and this
  // method (malformed requests), explicitly terminate the gpu.
  if (!connection_->single_pixel_buffer()) {
    error_message_ = "Single pixel buffer protocol is not available.";
    TerminateGpuProcess();
    return;
  }

  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingSinglePixel>(
                     connection_, color, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

WaylandBufferHandle* WaylandBufferManagerHost::EnsureBufferHandle(
    WaylandSurface* requestor,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());
  DCHECK(requestor);

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end())
    return nullptr;

  return it->second->EnsureBufferHandle(requestor);
}

WaylandBufferHandle* WaylandBufferManagerHost::GetBufferHandle(
    WaylandSurface* requestor,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(requestor);

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end())
    return nullptr;

  return it->second->GetBufferHandle(requestor);
}

uint32_t WaylandBufferManagerHost::GetBufferFormat(WaylandSurface* requestor,
                                                   uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(requestor);

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end())
    return DRM_FORMAT_INVALID;

  return it->second.get()->format();
}

void WaylandBufferManagerHost::CommitOverlays(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    const gfx::FrameData& data,
    std::vector<wl::WaylandOverlayConfig> overlays) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT0("wayland", "WaylandBufferManagerHost::CommitOverlays");

  DCHECK(error_message_.empty());

  if (widget == gfx::kNullAcceleratedWidget) {
    error_message_ = "Invalid widget.";
    TerminateGpuProcess();
  }
  WaylandWindow* window = connection_->window_manager()->GetWindow(widget);
  // In tab dragging, window may have been destroyed when buffers reach here. We
  // omit buffer commits and OnSubmission, because the corresponding buffer
  // queue in gpu process should be destroyed soon.
  if (!window)
    return;

  window->CommitOverlays(frame_id, data, overlays);
}

void WaylandBufferManagerHost::DestroyBuffer(uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::DestroyBuffer",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());
  if (!ValidateBufferExistence(buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  buffer_backings_.erase(buffer_id);
  dma_buffers_.erase(buffer_id);
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!ValidateDataFromGpu(size, buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = base::StrCat({"Number of strides(", NumberToString(strides.size()),
                           ")/offsets(", NumberToString(offsets.size()),
                           ")/modifiers(", NumberToString(modifiers.size()),
                           ") does not correspond to the number of planes(",
                           NumberToString(planes_count), ")"});
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManagerHost::ValidateBufferIdFromGpu(uint32_t buffer_id) {
  std::string reason;
  if (buffer_id < 1)
    reason = base::StrCat({"Invalid buffer id: ", NumberToString(buffer_id)});

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(const base::ScopedFD& fd,
                                                   size_t length,
                                                   const gfx::Size& size,
                                                   uint32_t buffer_id) {
  if (!ValidateDataFromGpu(size, buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (length == 0)
    reason = "The shm pool length cannot be less than 1";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(const gfx::Size& size,
                                                   uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (size.IsEmpty())
    error_message_ = "Buffer size is invalid";

  return error_message_.empty();
}

bool WaylandBufferManagerHost::ValidateBufferExistence(uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end()) {
    error_message_ = base::StrCat(
        {"Buffer with ", NumberToString(buffer_id), " id does not exist"});
  }

  return error_message_.empty();
}

void WaylandBufferManagerHost::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    const gfx::SwapResult& swap_result,
    gfx::GpuFenceHandle release_fence,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnSubmission(widget, frame_id, swap_result,
                                               std::move(release_fence),
                                               presentation_infos);
}

void WaylandBufferManagerHost::OnPresentation(
    gfx::AcceleratedWidget widget,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnPresentation(widget, presentation_infos);
}

void WaylandBufferManagerHost::InsertAcquireFence(uint32_t buffer_id,
                                                  int sync_fd) {
  DCHECK(connection_->UseImplicitSyncInterop());
  auto it = dma_buffers_.find(buffer_id);
  if (it == dma_buffers_.end()) {
    return;
  }

  struct dma_buf_import_sync_file req;
  req.flags = DMA_BUF_SYNC_RW;
  req.fd = sync_fd;

  int rv = HANDLE_EINTR(
      ioctl(it->second.get(), DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &req));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_IOCTL_IMPORT_SYNC_FILE";
}

base::ScopedFD WaylandBufferManagerHost::ExtractReleaseFence(
    uint32_t buffer_id) {
  DCHECK(connection_->UseImplicitSyncInterop());
  auto it = dma_buffers_.find(buffer_id);
  if (it == dma_buffers_.end()) {
    return base::ScopedFD();
  }

  struct dma_buf_export_sync_file req;
  req.flags = DMA_BUF_SYNC_RW;
  req.fd = -1;

  if (HANDLE_EINTR(
          ioctl(it->second.get(), DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &req)) < 0) {
    return base::ScopedFD();
  }

  return base::ScopedFD(req.fd);
}

// static
bool WaylandBufferManagerHost::SupportsImplicitSyncInterop() {
  static const bool can_import_export_sync_file = CheckImportExportFence();

  return can_import_export_sync_file;
}

void WaylandBufferManagerHost::TerminateGpuProcess() {
  DCHECK(!error_message_.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message_));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

}  // namespace ui

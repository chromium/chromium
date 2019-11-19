// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

using DrmEventHandler =
    base::RepeatingCallback<void(uint32_t /* frame */,
                                 base::TimeTicks /* timestamp */,
                                 uint64_t /* id */)>;

bool DrmCreateDumbBuffer(int fd,
                         const SkImageInfo& info,
                         uint32_t* handle,
                         uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VPLOG(2) << "Cannot create dumb buffer";
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.computeByteSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

bool DrmDestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  return !drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool ProcessDrmEvent(int fd, const DrmEventHandler& callback) {
  char buffer[1024];
  int len = read(fd, buffer, sizeof(buffer));
  if (len == 0)
    return false;

  if (len < static_cast<int>(sizeof(drm_event))) {
    PLOG(ERROR) << "Failed to read DRM event";
    return false;
  }

  int idx = 0;
  while (idx < len) {
    DCHECK_LE(static_cast<int>(sizeof(drm_event)), len - idx);
    drm_event event;
    memcpy(&event, &buffer[idx], sizeof(event));
    switch (event.type) {
      case DRM_EVENT_FLIP_COMPLETE: {
        DCHECK_LE(static_cast<int>(sizeof(drm_event_vblank)), len - idx);
        drm_event_vblank vblank;
        memcpy(&vblank, &buffer[idx], sizeof(vblank));
        std::unique_ptr<base::trace_event::TracedValue> drm_data(
            new base::trace_event::TracedValue());
        drm_data->SetInteger("frame_count", 1);
        drm_data->SetInteger("vblank.tv_sec", vblank.tv_sec);
        drm_data->SetInteger("vblank.tv_usec", vblank.tv_usec);
        TRACE_EVENT_INSTANT1("benchmark,drm", "DrmEventFlipComplete",
                             TRACE_EVENT_SCOPE_THREAD, "data",
                             std::move(drm_data));
        // Warning: It is generally unsafe to manufacture TimeTicks values; but
        // here it is required for interfacing with libdrm. Assumption: libdrm
        // is providing the timestamp from the CLOCK_MONOTONIC POSIX clock.
        DCHECK_EQ(base::TimeTicks::GetClock(),
                  base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
        const base::TimeTicks timestamp =
            base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                    static_cast<int64_t>(vblank.tv_sec) *
                                        base::Time::kMicrosecondsPerSecond +
                                    vblank.tv_usec);
        callback.Run(vblank.sequence, timestamp, vblank.user_data);
      } break;
      case DRM_EVENT_VBLANK:
        break;
      default:
        NOTREACHED();
        break;
    }

    idx += event.length;
  }

  return true;
}

bool CanQueryForResources(int fd) {
  drm_mode_card_res resources;
  memset(&resources, 0, sizeof(resources));
  // If there is no error getting DRM resources then assume this is a
  // modesetting device.
  return !drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources);
}

}  // namespace

DrmPropertyBlobMetadata::DrmPropertyBlobMetadata(DrmDevice* drm, uint32_t id)
    : drm_(drm), id_(id) {}

DrmPropertyBlobMetadata::~DrmPropertyBlobMetadata() {
  DCHECK(drm_);
  DCHECK(id_);
  drm_->DestroyPropertyBlob(id_);
}

class DrmDevice::PageFlipManager {
 public:
  PageFlipManager() : next_id_(0) {}
  ~PageFlipManager() {}

  void OnPageFlip(uint32_t frame, base::TimeTicks timestamp, uint64_t id) {
    auto it =
        std::find_if(callbacks_.begin(), callbacks_.end(), FindCallback(id));
    if (it == callbacks_.end()) {
      LOG(WARNING) << "Could not find callback for page flip id=" << id;
      return;
    }

    it->pending_calls--;
    if (it->pending_calls)
      return;

    DrmDevice::PageFlipCallback callback = std::move(it->callback);
    callbacks_.erase(it);
    std::move(callback).Run(frame, timestamp);
  }

  uint64_t GetNextId() { return next_id_++; }

  void RegisterCallback(uint64_t id,
                        uint64_t pending_calls,
                        DrmDevice::PageFlipCallback callback) {
    callbacks_.push_back({id, pending_calls, std::move(callback)});
  }

 private:
  struct PageFlip {
    uint64_t id;
    uint32_t pending_calls;
    DrmDevice::PageFlipCallback callback;
  };

  struct FindCallback {
    explicit FindCallback(uint64_t id) : id(id) {}

    bool operator()(const PageFlip& flip) const { return flip.id == id; }

    const uint64_t id;
  };

  uint64_t next_id_;

  std::vector<PageFlip> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipManager);
};

class DrmDevice::IOWatcher : public base::MessagePumpLibevent::FdWatcher {
 public:
  IOWatcher(int fd, DrmDevice::PageFlipManager* page_flip_manager)
      : page_flip_manager_(page_flip_manager), controller_(FROM_HERE), fd_(fd) {
    Register();
  }

  ~IOWatcher() override { Unregister(); }

 private:
  void Register() {
    DCHECK(base::MessageLoopCurrentForIO::IsSet());
    base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        fd_, true, base::MessagePumpForIO::WATCH_READ, &controller_, this);
  }

  void Unregister() {
    DCHECK(base::MessageLoopCurrentForIO::IsSet());
    controller_.StopWatchingFileDescriptor();
  }

  // base::MessagePumpLibevent::FdWatcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::MessageLoopCurrentForIO::IsSet());
    TRACE_EVENT1("drm", "OnDrmEvent", "socket", fd);

    if (!ProcessDrmEvent(
            fd, base::BindRepeating(&DrmDevice::PageFlipManager::OnPageFlip,
                                    base::Unretained(page_flip_manager_))))
      Unregister();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

  DrmDevice::PageFlipManager* page_flip_manager_;

  base::MessagePumpLibevent::FdWatchController controller_;

  int fd_;

  DISALLOW_COPY_AND_ASSIGN(IOWatcher);
};

DrmDevice::DrmDevice(const base::FilePath& device_path,
                     base::File file,
                     bool is_primary_device,
                     std::unique_ptr<GbmDevice> gbm)
    : device_path_(device_path),
      file_(std::move(file)),
      page_flip_manager_(new PageFlipManager()),
      is_primary_device_(is_primary_device),
      gbm_(std::move(gbm)) {}

DrmDevice::~DrmDevice() {}

bool DrmDevice::Initialize() {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(file_.GetPlatformFile())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

  // Use atomic only if kernel allows it.
  is_atomic_ = SetCapability(DRM_CLIENT_CAP_ATOMIC, 1);
  if (is_atomic_)
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerAtomic>(this);
  else
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
  if (!plane_manager_->Initialize()) {
    LOG(ERROR) << "Failed to initialize the plane manager for "
               << device_path_.value();
    plane_manager_.reset();
    return false;
  }

  uint64_t value;
  allow_addfb2_modifiers_ =
      GetCapability(DRM_CAP_ADDFB2_MODIFIERS, &value) && value;

  watcher_.reset(
      new IOWatcher(file_.GetPlatformFile(), page_flip_manager_.get()));

  return true;
}

ScopedDrmResourcesPtr DrmDevice::GetResources() {
  DCHECK(file_.IsValid());
  return ScopedDrmResourcesPtr(drmModeGetResources(file_.GetPlatformFile()));
}

ScopedDrmObjectPropertyPtr DrmDevice::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) {
  DCHECK(file_.IsValid());
  return ScopedDrmObjectPropertyPtr(drmModeObjectGetProperties(
      file_.GetPlatformFile(), object_id, object_type));
}

ScopedDrmCrtcPtr DrmDevice::GetCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(file_.GetPlatformFile(), crtc_id));
}

bool DrmDevice::SetCrtc(uint32_t crtc_id,
                        uint32_t framebuffer,
                        std::vector<uint32_t> connectors,
                        drmModeModeInfo* mode) {
  DCHECK(file_.IsValid());
  DCHECK(!connectors.empty());
  DCHECK(mode);

  TRACE_EVENT2("drm", "DrmDevice::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode->hdisplay, mode->vdisplay).ToString());
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, framebuffer, 0, 0,
                         connectors.data(), connectors.size(), mode);
}

bool DrmDevice::SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) {
  DCHECK(file_.IsValid());
  // If there's no buffer then the CRTC was disabled.
  if (!crtc->buffer_id)
    return DisableCrtc(crtc->crtc_id);

  DCHECK(!connectors.empty());

  TRACE_EVENT1("drm", "DrmDevice::RestoreCrtc", "crtc", crtc->crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc->crtc_id,
                         crtc->buffer_id, crtc->x, crtc->y, connectors.data(),
                         connectors.size(), &crtc->mode);
}

bool DrmDevice::DisableCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, 0, 0, 0, NULL, 0,
                         NULL);
}

ScopedDrmConnectorPtr DrmDevice::GetConnector(uint32_t connector_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(file_.GetPlatformFile(), connector_id));
}

bool DrmDevice::AddFramebuffer2(uint32_t width,
                                uint32_t height,
                                uint32_t format,
                                uint32_t handles[4],
                                uint32_t strides[4],
                                uint32_t offsets[4],
                                uint64_t modifiers[4],
                                uint32_t* framebuffer,
                                uint32_t flags) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::AddFramebuffer", "handle", handles[0]);
  return !drmModeAddFB2WithModifiers(file_.GetPlatformFile(), width, height,
                                     format, handles, strides, offsets,
                                     modifiers, framebuffer, flags);
}

bool DrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(file_.GetPlatformFile(), framebuffer);
}

bool DrmDevice::PageFlip(uint32_t crtc_id,
                         uint32_t framebuffer,
                         scoped_refptr<PageFlipRequest> page_flip_request) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlip", "crtc", crtc_id, "framebuffer",
               framebuffer);

  // NOTE: Calling drmModeSetCrtc will immediately update the state, though
  // callbacks to already scheduled page flips will be honored by the kernel.
  uint64_t id = page_flip_manager_->GetNextId();
  if (!drmModePageFlip(file_.GetPlatformFile(), crtc_id, framebuffer,
                       DRM_MODE_PAGE_FLIP_EVENT, reinterpret_cast<void*>(id))) {
    // If successful the payload will be removed by a PageFlip event.
    page_flip_manager_->RegisterCallback(id, 1,
                                         page_flip_request->AddPageFlip());
    return true;
  }

  return false;
}

ScopedDrmFramebufferPtr DrmDevice::GetFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(
      drmModeGetFB(file_.GetPlatformFile(), framebuffer));
}

ScopedDrmPlanePtr DrmDevice::GetPlane(uint32_t plane_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmPlanePtr(drmModeGetPlane(file_.GetPlatformFile(), plane_id));
}

ScopedDrmPlaneResPtr DrmDevice::GetPlaneResources() {
  DCHECK(file_.IsValid());
  return ScopedDrmPlaneResPtr(
      drmModeGetPlaneResources(file_.GetPlatformFile()));
}

ScopedDrmPropertyPtr DrmDevice::GetProperty(drmModeConnector* connector,
                                            const char* name) {
  TRACE_EVENT2("drm", "DrmDevice::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property;
  }

  return ScopedDrmPropertyPtr();
}

ScopedDrmPropertyPtr DrmDevice::GetProperty(uint32_t id) {
  return ScopedDrmPropertyPtr(drmModeGetProperty(file_.GetPlatformFile(), id));
}

bool DrmDevice::SetProperty(uint32_t connector_id,
                            uint32_t property_id,
                            uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmModeConnectorSetProperty(file_.GetPlatformFile(), connector_id,
                                      property_id, value);
}

ScopedDrmPropertyBlob DrmDevice::CreatePropertyBlob(void* blob, size_t size) {
  uint32_t id = 0;
  int ret = drmModeCreatePropertyBlob(file_.GetPlatformFile(), blob, size, &id);
  DCHECK(!ret && id);

  return ScopedDrmPropertyBlob(new DrmPropertyBlobMetadata(this, id));
}

void DrmDevice::DestroyPropertyBlob(uint32_t id) {
  drmModeDestroyPropertyBlob(file_.GetPlatformFile(), id);
}

bool DrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  DCHECK(file_.IsValid());
  return !drmGetCap(file_.GetPlatformFile(), capability, value);
}

ScopedDrmPropertyBlobPtr DrmDevice::GetPropertyBlob(uint32_t property_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmPropertyBlobPtr(
      drmModeGetPropertyBlob(file_.GetPlatformFile(), property_id));
}

ScopedDrmPropertyBlobPtr DrmDevice::GetPropertyBlob(drmModeConnector* connector,
                                                    const char* name) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        (property->flags & DRM_MODE_PROP_BLOB))
      return ScopedDrmPropertyBlobPtr(drmModeGetPropertyBlob(
          file_.GetPlatformFile(), connector->prop_values[i]));
  }

  return ScopedDrmPropertyBlobPtr();
}

bool DrmDevice::SetObjectProperty(uint32_t object_id,
                                  uint32_t object_type,
                                  uint32_t property_id,
                                  uint32_t property_value) {
  DCHECK(file_.IsValid());
  return !drmModeObjectSetProperty(file_.GetPlatformFile(), object_id,
                                   object_type, property_id, property_value);
}

bool DrmDevice::SetCursor(uint32_t crtc_id,
                          uint32_t handle,
                          const gfx::Size& size) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::SetCursor", "crtc_id", crtc_id, "handle",
               handle);
  return !drmModeSetCursor(file_.GetPlatformFile(), crtc_id, handle,
                           size.width(), size.height());
}

bool DrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::MoveCursor", "crtc_id", crtc_id);
  return !drmModeMoveCursor(file_.GetPlatformFile(), crtc_id, point.x(),
                            point.y());
}

bool DrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                 uint32_t* handle,
                                 uint32_t* stride) {
  DCHECK(file_.IsValid());

  TRACE_EVENT0("drm", "DrmDevice::CreateDumbBuffer");
  return DrmCreateDumbBuffer(file_.GetPlatformFile(), info, handle, stride);
}

bool DrmDevice::DestroyDumbBuffer(uint32_t handle) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DestroyDumbBuffer", "handle", handle);
  return DrmDestroyDumbBuffer(file_.GetPlatformFile(), handle);
}

bool DrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_MODE_MAP_DUMB,
               &map_request)) {
    PLOG(ERROR) << "Cannot prepare dumb buffer for mapping";
    return false;
  }

  *pixels = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 file_.GetPlatformFile(), map_request.offset);
  if (*pixels == MAP_FAILED) {
    PLOG(ERROR) << "Cannot mmap dumb buffer";
    return false;
  }

  return true;
}

bool DrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return !munmap(pixels, size);
}

bool DrmDevice::CloseBufferHandle(uint32_t handle) {
  struct drm_gem_close close_request;
  memset(&close_request, 0, sizeof(close_request));
  close_request.handle = handle;
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_GEM_CLOSE,
                   &close_request);
}

bool DrmDevice::CommitProperties(
    drmModeAtomicReq* properties,
    uint32_t flags,
    uint32_t crtc_count,
    scoped_refptr<PageFlipRequest> page_flip_request) {
  uint64_t id = 0;
  if (page_flip_request) {
    flags |= DRM_MODE_PAGE_FLIP_EVENT;
    id = page_flip_manager_->GetNextId();
  }

  if (!drmModeAtomicCommit(file_.GetPlatformFile(), properties, flags,
                           reinterpret_cast<void*>(id))) {
    if (page_flip_request) {
      page_flip_manager_->RegisterCallback(id, crtc_count,
                                           page_flip_request->AddPageFlip());
    }

    return true;
  }
  return false;
}

bool DrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  DCHECK(file_.IsValid());

  struct drm_set_client_cap cap = {capability, value};
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_SET_CLIENT_CAP, &cap);
}

bool DrmDevice::SetMaster() {
  TRACE_EVENT1("drm", "DrmDevice::SetMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmSetMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::DropMaster() {
  TRACE_EVENT1("drm", "DrmDevice::DropMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmDropMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::SetGammaRamp(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& lut) {
  ScopedDrmCrtcPtr crtc = GetCrtc(crtc_id);
  size_t gamma_size = static_cast<size_t>(crtc->gamma_size);

  if (gamma_size == 0 && lut.empty())
    return true;

  if (gamma_size == 0) {
    LOG(ERROR) << "Gamma table not supported";
    return false;
  }

  // TODO(robert.bradford) resample the incoming ramp to match what the kernel
  // expects.
  if (!lut.empty() && gamma_size != lut.size()) {
    LOG(ERROR) << "Gamma table size mismatch: supplied " << lut.size()
               << " expected " << gamma_size;
    return false;
  }

  std::vector<uint16_t> r, g, b;
  r.reserve(gamma_size);
  g.reserve(gamma_size);
  b.reserve(gamma_size);

  if (lut.empty()) {
    // Create a linear gamma ramp table to deactivate the feature.
    for (size_t i = 0; i < gamma_size; ++i) {
      uint16_t value = (i * ((1 << 16) - 1)) / (gamma_size - 1);
      r.push_back(value);
      g.push_back(value);
      b.push_back(value);
    }
  } else {
    for (size_t i = 0; i < gamma_size; ++i) {
      r.push_back(lut[i].r);
      g.push_back(lut[i].g);
      b.push_back(lut[i].b);
    }
  }

  DCHECK(file_.IsValid());
  TRACE_EVENT0("drm", "DrmDevice::SetGamma");
  return (drmModeCrtcSetGamma(file_.GetPlatformFile(), crtc_id, r.size(), &r[0],
                              &g[0], &b[0]) == 0);
}

}  // namespace ui

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

using DrmEventHandler =
    base::RepeatingCallback<void(uint32_t /* frame */,
                                 base::TimeTicks /* timestamp */,
                                 uint64_t /* id */)>;

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
            base::TimeTicks() +
            base::Microseconds(static_cast<int64_t>(vblank.tv_sec) *
                                   base::Time::kMicrosecondsPerSecond +
                               vblank.tv_usec);
        callback.Run(vblank.sequence, timestamp, vblank.user_data);
      } break;
      case DRM_EVENT_VBLANK:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    idx += event.length;
  }

  return true;
}

}  // namespace

class DrmDevice::PageFlipManager {
 public:
  PageFlipManager() = default;

  PageFlipManager(const PageFlipManager&) = delete;
  PageFlipManager& operator=(const PageFlipManager&) = delete;

  ~PageFlipManager() = default;

  void OnPageFlip(uint32_t frame, base::TimeTicks timestamp, uint64_t id) {
    auto it = base::ranges::find(callbacks_, id, &PageFlip::id);
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
    callbacks_.push_back(
        {id, static_cast<uint32_t>(pending_calls), std::move(callback)});
  }

 private:
  struct PageFlip {
    uint64_t id;
    uint32_t pending_calls;
    DrmDevice::PageFlipCallback callback;
  };

  uint64_t next_id_ = 0;

  std::vector<PageFlip> callbacks_;
};

class DrmDevice::IOWatcher : public base::MessagePumpEpoll::FdWatcher {
 public:
  IOWatcher(int fd, DrmDevice::PageFlipManager* page_flip_manager)
      : page_flip_manager_(page_flip_manager), controller_(FROM_HERE), fd_(fd) {
    Register();
  }

  IOWatcher(const IOWatcher&) = delete;
  IOWatcher& operator=(const IOWatcher&) = delete;

  ~IOWatcher() override { Unregister(); }

 private:
  void Register() {
    DCHECK(base::CurrentIOThread::IsSet());
    base::CurrentIOThread::Get()->WatchFileDescriptor(
        fd_, true, base::MessagePumpForIO::WATCH_READ, &controller_, this);
  }

  void Unregister() {
    DCHECK(base::CurrentIOThread::IsSet());
    controller_.StopWatchingFileDescriptor();
  }

  // base::MessagePumpEpoll::FdWatcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::CurrentIOThread::IsSet());
    TRACE_EVENT1("drm", "OnDrmEvent", "socket", fd);

    if (!ProcessDrmEvent(
            fd, base::BindRepeating(&DrmDevice::PageFlipManager::OnPageFlip,
                                    base::Unretained(page_flip_manager_))))
      Unregister();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    NOTREACHED_IN_MIGRATION();
  }

  raw_ptr<DrmDevice::PageFlipManager> page_flip_manager_;

  base::MessagePumpEpoll::FdWatchController controller_;

  int fd_;
};

DrmDevice::DrmDevice(const base::FilePath& device_path,
                     base::ScopedFD fd,
                     bool is_primary_device,
                     std::unique_ptr<GbmDevice> gbm)
    : DrmWrapper(device_path, std::move(fd), is_primary_device),
      page_flip_manager_(new PageFlipManager()),
      gbm_(std::move(gbm)) {}

DrmDevice::~DrmDevice() = default;

bool DrmDevice::Initialize() {
  if (!DrmWrapper::Initialize()) {
    return false;
  }

  // Use atomic only if kernel allows it.
  if (is_atomic()) {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerAtomic>(this);
  } else {
    plane_manager_ = std::make_unique<HardwareDisplayPlaneManagerLegacy>(this);
  }

  if (!plane_manager_->Initialize()) {
    LOG(ERROR) << "Failed to initialize the plane manager for "
               << device_path().value();
    plane_manager_.reset();
    return false;
  }

  watcher_ = std::make_unique<IOWatcher>(GetFd(), page_flip_manager_.get());

  return true;
}

bool DrmDevice::SetCrtc(uint32_t crtc_id,
                        uint32_t framebuffer,
                        std::vector<uint32_t> connectors,
                        const drmModeModeInfo& mode) {
  if (!DrmWrapper::SetCrtc(crtc_id, framebuffer, connectors, mode))
    return false;

  ++modeset_sequence_id_;
  return true;
}

bool DrmDevice::PageFlip(uint32_t crtc_id,
                         uint32_t framebuffer,
                         scoped_refptr<PageFlipRequest> page_flip_request) {
  const uint64_t id = page_flip_manager_->GetNextId();
  if (!DrmWrapper::PageFlip(crtc_id, framebuffer, id))
    return false;

  // If successful the payload will be removed by a PageFlip event.
  page_flip_manager_->RegisterCallback(id, 1, page_flip_request->AddPageFlip());
  return true;
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

  if (!DrmWrapper::CommitProperties(properties, flags, id))
    return false;

  if (page_flip_request) {
    page_flip_manager_->RegisterCallback(id, crtc_count,
                                         page_flip_request->AddPageFlip());
  }

  if (flags == DRM_MODE_ATOMIC_ALLOW_MODESET)
    ++modeset_sequence_id_;

  return true;
}

void DrmDevice::WriteIntoTrace(perfetto::TracedDictionary dict) const {
  dict.Add("planes", plane_manager_->planes());
  DrmWrapper::WriteIntoTrace(std::move(dict));
}

display::DrmFormatsAndModifiers DrmDevice::GetFormatsAndModifiersForCrtc(
    uint32_t crtc_id) const {
  display::DrmFormatsAndModifiers drm_formats_and_modifiers;
  for (uint32_t format : plane_manager_->GetSupportedFormats()) {
    std::vector<uint64_t> modifiers =
        plane_manager_->GetFormatModifiers(crtc_id, format);
    drm_formats_and_modifiers.emplace(format, modifiers);
  }
  return drm_formats_and_modifiers;
}

int DrmDevice::modeset_sequence_id() const { return modeset_sequence_id_; }

}  // namespace ui

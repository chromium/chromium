// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_shm_image_pool.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/url_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/extension_manager.h"

namespace ui {

namespace {

constexpr int kMinImageAreaForShmem = 256;

// When resizing a segment, the new segment size is calculated as
//   new_size = target_size * kShmResizeThreshold
// so that target_size has room to grow before another resize is necessary.  We
// also want target_size to have room to shrink, so we avoid resizing until
//   shrink_size = target_size / kShmResizeThreshold
// Given these equations, shrink_size is
//   shrink_size = new_size / kShmResizeThreshold ^ 2
// new_size is recorded in SoftwareOutputDeviceX11::shm_size_, so we need to
// divide by kShmResizeThreshold twice to get the shrink threshold.
constexpr float kShmResizeThreshold = 1.5f;
constexpr float kShmResizeShrinkThreshold =
    1.0f / (kShmResizeThreshold * kShmResizeThreshold);

std::size_t MaxShmSegmentSizeImpl() {
  struct shminfo info;
  if (shmctl(0, IPC_INFO, reinterpret_cast<struct shmid_ds*>(&info)) == -1)
    return 0;
  return info.shmmax;
}

std::size_t MaxShmSegmentSize() {
  static std::size_t max_size = MaxShmSegmentSizeImpl();
  return max_size;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool IsRemoteHost(const std::string& name) {
  if (name.empty())
    return false;

  return !net::HostStringIsLocalhost(name);
}

bool ShouldUseMitShm(x11::Connection* connection) {
  // MIT-SHM may be available on remote connetions, but it will be unusable.  Do
  // a best-effort check to see if the host is remote to disable the SHM
  // codepath.  It may be possible in contrived cases for there to be a
  // false-positive, but in that case we'll just fallback to the non-SHM
  // codepath.
  auto host = connection->GetConnectionHostname();
  if (!host.empty() && IsRemoteHost(host))
    return false;

  std::unique_ptr<base::Environment> env = base::Environment::Create();

  // Used by QT.
  if (env->HasVar("QT_X11_NO_MITSHM"))
    return false;

  // Used by JRE.
  std::string j2d_use_mitshm;
  if (env->GetVar("J2D_USE_MITSHM", &j2d_use_mitshm) &&
      (j2d_use_mitshm == "0" ||
       base::EqualsCaseInsensitiveASCII(j2d_use_mitshm, "false"))) {
    return false;
  }

  // Used by GTK.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoXshm))
    return false;

  return true;
}
#endif

}  // namespace

XShmImagePool::FrameState::FrameState() = default;

XShmImagePool::FrameState::~FrameState() = default;

XShmImagePool::SwapClosure::SwapClosure() = default;

XShmImagePool::SwapClosure::~SwapClosure() = default;

XShmImagePool::XShmImagePool(x11::Connection* connection,
                             x11::Drawable drawable,
                             x11::VisualId visual,
                             int depth,
                             std::size_t frames_pending,
                             bool enable_multibuffering)
    : connection_(connection),
      drawable_(drawable),
      visual_(visual),
      depth_(depth),
      enable_multibuffering_(enable_multibuffering),
      frame_states_(frames_pending) {
  if (enable_multibuffering_)
    connection_->AddEventObserver(this);
}

XShmImagePool::~XShmImagePool() {
  Cleanup();
  if (enable_multibuffering_)
    connection_->RemoveEventObserver(this);
}

bool XShmImagePool::Resize(const gfx::Size& pixel_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pixel_size == pixel_size_)
    return true;

  auto cleanup_fn = [](XShmImagePool* x) { x->Cleanup(); };
  std::unique_ptr<XShmImagePool, decltype(cleanup_fn)> cleanup{this,
                                                               cleanup_fn};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ShouldUseMitShm(connection_))
    return false;
#endif

  if (!ui::QueryShmSupport())
    return false;

  if (pixel_size.width() <= 0 || pixel_size.height() <= 0 ||
      pixel_size.GetArea() <= kMinImageAreaForShmem) {
    return false;
  }

  SkColorType color_type = ColorTypeForVisual(visual_);
  if (color_type == kUnknown_SkColorType)
    return false;

  SkImageInfo image_info = SkImageInfo::Make(
      pixel_size.width(), pixel_size.height(), color_type, kPremul_SkAlphaType);
  std::size_t needed_frame_bytes = image_info.computeMinByteSize();

  if (needed_frame_bytes > frame_bytes_ ||
      needed_frame_bytes < frame_bytes_ * kShmResizeShrinkThreshold) {
    // Resize.
    Cleanup();

    frame_bytes_ = needed_frame_bytes * kShmResizeThreshold;
    if (MaxShmSegmentSize() > 0 && frame_bytes_ > MaxShmSegmentSize()) {
      if (MaxShmSegmentSize() >= needed_frame_bytes)
        frame_bytes_ = MaxShmSegmentSize();
      else
        return false;
    }

    for (FrameState& state : frame_states_) {
      state.shmid =
          shmget(IPC_PRIVATE, frame_bytes_,
                 IPC_CREAT | SHM_R | SHM_W | (SHM_R >> 6) | (SHM_W >> 6));
      if (state.shmid < 0)
        return false;
      state.shmaddr = reinterpret_cast<char*>(shmat(state.shmid, nullptr, 0));
      if (state.shmaddr == reinterpret_cast<char*>(-1)) {
        shmctl(state.shmid, IPC_RMID, nullptr);
        return false;
      }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // On Linux, a shmid can still be attached after IPC_RMID if otherwise
      // kept alive.  Detach before XShmAttach to prevent a memory leak in case
      // the process dies.
      shmctl(state.shmid, IPC_RMID, nullptr);
#endif
      DCHECK(!state.shmem_attached_to_server);
      auto shmseg = connection_->GenerateId<x11::Shm::Seg>();
      auto req = connection_->shm().Attach({
          .shmseg = shmseg,
          .shmid = static_cast<uint32_t>(state.shmid),
          // If this class ever needs to use XShmGetImage(), this needs to be
          // changed to read-write.
          .read_only = true,
      });
      if (req.Sync().error)
        return false;
      state.shmseg = shmseg;
      state.shmem_attached_to_server = true;
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
      // The Linux-specific shmctl behavior above may not be portable, so we're
      // forced to do IPC_RMID after the server has attached to the segment.
      shmctl(state.shmid, IPC_RMID, nullptr);
#endif
    }
  }

  const auto* visual_info = connection_->GetVisualInfoFromId(visual_);
  if (!visual_info)
    return false;
  size_t row_bytes = RowBytesForVisualWidth(*visual_info, pixel_size.width());

  for (FrameState& state : frame_states_) {
    state.bitmap = SkBitmap();
    if (!state.bitmap.installPixels(image_info, state.shmaddr, row_bytes))
      return false;
    state.canvas = std::make_unique<SkCanvas>(state.bitmap);
  }

  pixel_size_ = pixel_size;
  cleanup.release();
  ready_ = true;
  return true;
}

bool XShmImagePool::Ready() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ready_;
}

SkBitmap& XShmImagePool::CurrentBitmap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return frame_states_[current_frame_index_].bitmap;
}

SkCanvas* XShmImagePool::CurrentCanvas() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return frame_states_[current_frame_index_].canvas.get();
}

x11::Shm::Seg XShmImagePool::CurrentSegment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return frame_states_[current_frame_index_].shmseg;
}

void XShmImagePool::SwapBuffers(
    base::OnceCallback<void(const gfx::Size&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enable_multibuffering_);

  swap_closures_.emplace_back();
  SwapClosure& swap_closure = swap_closures_.back();
  swap_closure.closure = base::BindOnce(std::move(callback), pixel_size_);
  swap_closure.shmseg = frame_states_[current_frame_index_].shmseg;

  current_frame_index_ = (current_frame_index_ + 1) % frame_states_.size();
}

void XShmImagePool::DispatchShmCompletionEvent(
    x11::Shm::CompletionEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.offset, 0UL);
  DCHECK(enable_multibuffering_);

  for (auto it = swap_closures_.begin(); it != swap_closures_.end(); ++it) {
    if (event.shmseg == it->shmseg) {
      std::move(it->closure).Run();
      swap_closures_.erase(it);
      return;
    }
  }
}

void XShmImagePool::OnEvent(const x11::Event& xev) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enable_multibuffering_);

  auto* completion = xev.As<x11::Shm::CompletionEvent>();
  if (completion && completion->drawable.value == drawable_.value)
    DispatchShmCompletionEvent(*completion);
}

void XShmImagePool::Cleanup() {
  for (FrameState& state : frame_states_) {
    if (state.shmaddr)
      shmdt(state.shmaddr.ExtractAsDangling());
    if (state.shmem_attached_to_server)
      connection_->shm().Detach({state.shmseg});
    state.shmem_attached_to_server = false;
    state.shmseg = x11::Shm::Seg{};
    state.shmid = 0;
    state.shmaddr = nullptr;
  }
  frame_bytes_ = 0;
  pixel_size_ = gfx::Size();
  current_frame_index_ = 0;
  ready_ = false;
}

}  // namespace ui

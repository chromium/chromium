// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_shm_image_pool_base.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_switches.h"

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

#if !defined(OS_CHROMEOS)
bool ShouldUseMitShm() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();

  // Used by QT.
  if (env->HasVar("QT_X11_NO_MITSHM"))
    return false;

  // Used by JRE.
  std::string j2d_use_mitshm;
  if (env->GetVar("J2D_USE_MITSHM", &j2d_use_mitshm) &&
      (j2d_use_mitshm == "0" ||
       base::LowerCaseEqualsASCII(j2d_use_mitshm, "false"))) {
    return false;
  }

  // Used by GTK.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoXshm))
    return false;

  return true;
}
#endif

}  // namespace

XShmImagePoolBase::FrameState::FrameState() = default;

XShmImagePoolBase::FrameState::~FrameState() = default;

XShmImagePoolBase::SwapClosure::SwapClosure() = default;

XShmImagePoolBase::SwapClosure::~SwapClosure() = default;

XShmImagePoolBase::XShmImagePoolBase(base::TaskRunner* host_task_runner,
                                     base::TaskRunner* event_task_runner,
                                     XDisplay* display,
                                     XID drawable,
                                     Visual* visual,
                                     int depth,
                                     std::size_t frames_pending)
    : host_task_runner_(host_task_runner),
      event_task_runner_(event_task_runner),
      display_(display),
      drawable_(drawable),
      visual_(visual),
      depth_(depth),
      frame_states_(frames_pending) {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());
}

bool XShmImagePoolBase::Resize(const gfx::Size& pixel_size) {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  if (pixel_size == pixel_size_)
    return true;

  auto cleanup_fn = [](XShmImagePoolBase* x) { x->Cleanup(); };
  std::unique_ptr<XShmImagePoolBase, decltype(cleanup_fn)> cleanup{this,
                                                                   cleanup_fn};

  if (!event_task_runner_)
    return false;

#if !defined(OS_CHROMEOS)
  if (!ShouldUseMitShm())
    return false;
#endif

  if (!ui::QueryShmSupport())
    return false;

  if (pixel_size.width() <= 0 || pixel_size.height() <= 0 ||
      pixel_size.GetArea() <= kMinImageAreaForShmem) {
    return false;
  }

  std::size_t needed_frame_bytes;
  for (std::size_t i = 0; i < frame_states_.size(); ++i) {
    FrameState& state = frame_states_[i];
    state.image.reset(XShmCreateImage(display_, visual_, depth_, ZPixmap,
                                      nullptr, &state.shminfo_,
                                      pixel_size.width(), pixel_size.height()));
    if (!state.image)
      return false;
    std::size_t current_frame_bytes =
        state.image->bytes_per_line * state.image->height;
    if (i == 0)
      needed_frame_bytes = current_frame_bytes;
    else
      DCHECK_EQ(current_frame_bytes, needed_frame_bytes);
  }

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
      state.shminfo_.shmid =
          shmget(IPC_PRIVATE, frame_bytes_,
                 IPC_CREAT | SHM_R | SHM_W | (SHM_R >> 6) | (SHM_W >> 6));
      if (state.shminfo_.shmid < 0)
        return false;
      state.shminfo_.shmaddr =
          reinterpret_cast<char*>(shmat(state.shminfo_.shmid, 0, 0));
      if (state.shminfo_.shmaddr == reinterpret_cast<char*>(-1)) {
        shmctl(state.shminfo_.shmid, IPC_RMID, 0);
        return false;
      }
#if defined(OS_LINUX)
      // On Linux, a shmid can still be attached after IPC_RMID if otherwise
      // kept alive.  Detach before XShmAttach to prevent a memory leak in case
      // the process dies.
      shmctl(state.shminfo_.shmid, IPC_RMID, 0);
#endif
      DCHECK(!state.shmem_attached_to_server_);
      if (!XShmAttach(display_, &state.shminfo_))
        return false;
      state.shmem_attached_to_server_ = true;
#if !defined(OS_LINUX)
      // The Linux-specific shmctl behavior above may not be portable, so we're
      // forced to do IPC_RMID after the server has attached to the segment.
      // XShmAttach is asynchronous, so we must also sync.
      XSync(display_, x11::False);
      shmctl(shminfo_.shmid, IPC_RMID, 0);
#endif
      // If this class ever needs to use XShmGetImage(), this needs to be
      // changed to read-write.
      state.shminfo_.readOnly = true;
    }
  }

  for (std::size_t i = 0; i < frame_states_.size(); ++i) {
    FrameState& state = frame_states_[i];
    state.image->data = state.shminfo_.shmaddr;
    SkImageInfo image_info =
        SkImageInfo::Make(state.image->width, state.image->height,
                          ColorTypeForVisual(visual_), kPremul_SkAlphaType);
    state.bitmap = SkBitmap();
    if (!state.bitmap.installPixels(image_info, state.image->data,
                                    state.image->bytes_per_line)) {
      return false;
    }
    state.canvas = std::make_unique<SkCanvas>(state.bitmap);
  }

  pixel_size_ = pixel_size;
  cleanup.release();
  ready_ = true;
  return true;
}

bool XShmImagePoolBase::Ready() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());
  return ready_;
}

SkBitmap& XShmImagePoolBase::CurrentBitmap() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  return frame_states_[current_frame_index_].bitmap;
}

SkCanvas* XShmImagePoolBase::CurrentCanvas() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  return frame_states_[current_frame_index_].canvas.get();
}

XImage* XShmImagePoolBase::CurrentImage() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  return frame_states_[current_frame_index_].image.get();
}

void XShmImagePoolBase::SwapBuffers(
    base::OnceCallback<void(const gfx::Size&)> callback) {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  swap_closures_.emplace();
  SwapClosure& swap_closure = swap_closures_.back();
  swap_closure.closure = base::BindOnce(std::move(callback), pixel_size_);
#ifndef NDEBUG
  swap_closure.shmseg = frame_states_[current_frame_index_].shminfo_.shmseg;
#endif

  current_frame_index_ = (current_frame_index_ + 1) % frame_states_.size();
}

void XShmImagePoolBase::Initialize() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());
  if (event_task_runner_)
    event_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&XShmImagePoolBase::InitializeOnGpu, this));
}

void XShmImagePoolBase::Teardown() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());
  if (event_task_runner_)
    event_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&XShmImagePoolBase::TeardownOnGpu, this));
}

XShmImagePoolBase::~XShmImagePoolBase() {
  Cleanup();
#ifndef NDEBUG
  DCHECK(!dispatcher_registered_);
#endif
}

void XShmImagePoolBase::DispatchShmCompletionEvent(XShmCompletionEvent event) {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!swap_closures_.empty());

  SwapClosure& swap_ack = swap_closures_.front();
#ifndef NDEBUG
  DCHECK_EQ(event.shmseg, swap_ack.shmseg);
  DCHECK_EQ(event.offset, 0UL);
#endif

  std::move(swap_ack.closure).Run();
  swap_closures_.pop();
}

bool XShmImagePoolBase::CanDispatchXEvent(XEvent* xev) {
  DCHECK(event_task_runner_->RunsTasksInCurrentSequence());

  if (xev->type != ui::ShmEventBase() + ShmCompletion)
    return false;

  XShmCompletionEvent* shm_event = reinterpret_cast<XShmCompletionEvent*>(xev);
  return shm_event->drawable == drawable_;
}

void XShmImagePoolBase::InitializeOnGpu() {
  DCHECK(event_task_runner_->RunsTasksInCurrentSequence());
  AddEventDispatcher();
#ifndef NDEBUG
  DCHECK(dispatcher_registered_);
#endif
}

void XShmImagePoolBase::TeardownOnGpu() {
  DCHECK(event_task_runner_->RunsTasksInCurrentSequence());
  RemoveEventDispatcher();
#ifndef NDEBUG
  DCHECK(!dispatcher_registered_);
#endif
}

void XShmImagePoolBase::Cleanup() {
  for (FrameState& state : frame_states_) {
    if (state.shminfo_.shmaddr)
      shmdt(state.shminfo_.shmaddr);
    if (state.shmem_attached_to_server_)
      XShmDetach(display_, &state.shminfo_);
    state.shmem_attached_to_server_ = false;
    state.shminfo_ = {};
  }
  frame_bytes_ = 0;
  pixel_size_ = gfx::Size();
  current_frame_index_ = 0;
  ready_ = false;
}

}  // namespace ui

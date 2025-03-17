// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/stack_allocated.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/synchronization/lock_subtle.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/win/windows_version.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/vsync_thread_win_dcomp.h"
#include "ui/gl/vsync_thread_win_dxgi.h"

namespace gl {
namespace {
// Whether the current thread holds the `VSyncThreadWin` lock.
thread_local bool g_current_thread_holds_lock = false;
}  // namespace

// static
VSyncThreadWin* VSyncThreadWin::GetInstance() {
  static VSyncThreadWin* vsync_thread = []() -> VSyncThreadWin* {
    if (features::UseCompositorClockVSyncInterval()) {
      return new VSyncThreadWinDComp();
    } else {
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device(
          GetDirectCompositionD3D11Device());
      if (!d3d11_device) {
        return nullptr;
      }
      Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
      CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);
      return new VSyncThreadWinDXGI(std::move(dxgi_device));
    }
  }();
  return vsync_thread;
}

class VSyncThreadWin::AutoVSyncThreadLock {
  STACK_ALLOCATED();

 public:
  AutoVSyncThreadLock(VSyncThreadWin* vsync_thread)
      EXCLUSIVE_LOCK_FUNCTION(vsync_thread->lock_) {
    if (g_current_thread_holds_lock) {
      vsync_thread->lock_.AssertAcquired();
    } else {
      auto_lock_.emplace(
          vsync_thread->lock_,
          // This lock is used to satisfy a mutual exclusion guarantee verified
          // by a SEQUENCE_CHECKER in `observers_`.
          base::subtle::LockTracking::kEnabled);
      g_current_thread_holds_lock = true;
    }
  }

  ~AutoVSyncThreadLock() UNLOCK_FUNCTION() {
    DCHECK(g_current_thread_holds_lock);
    if (auto_lock_.has_value()) {
      g_current_thread_holds_lock = false;
    }
  }

 private:
  std::optional<base::AutoLock> auto_lock_;
};

VSyncThreadWin::VSyncThreadWin() : vsync_thread_("GpuVSyncThread") {
  is_suspended_ = base::PowerMonitor::GetInstance()
                      ->AddPowerSuspendObserverAndReturnSuspendedState(this);
  vsync_thread_.StartWithOptions(
      base::Thread::Options(base::ThreadType::kDisplayCritical));
}

VSyncThreadWin::~VSyncThreadWin() {
  NOTREACHED();
}

void VSyncThreadWin::PostTaskIfNeeded() {
  lock_.AssertAcquired();
  // PostTaskIfNeeded is called from AddObserver and OnResume.
  // Before queuing up a task, make sure that there are observers waiting for
  // VSync and that we're not already firing events to consumers. Avoid firing
  // events if we're suspended to conserve battery life.
  if (!is_vsync_task_posted_ && !observers_.empty() && !is_suspended_) {
    vsync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VSyncThreadWin::WaitForVSync, base::Unretained(this)));
    is_vsync_task_posted_ = true;
  }
}

void VSyncThreadWin::AddObserver(VSyncObserver* obs) {
  AutoVSyncThreadLock auto_lock(this);
  observers_.AddObserver(obs);
  PostTaskIfNeeded();
}

void VSyncThreadWin::RemoveObserver(VSyncObserver* obs) {
  AutoVSyncThreadLock auto_lock(this);
  observers_.RemoveObserver(obs);
}

void VSyncThreadWin::OnSuspend() {
  AutoVSyncThreadLock auto_lock(this);
  is_suspended_ = true;
}

void VSyncThreadWin::OnResume() {
  AutoVSyncThreadLock auto_lock(this);
  is_suspended_ = false;
  PostTaskIfNeeded();
}

void VSyncThreadWin::WaitForVSync() {
  base::TimeDelta vsync_interval;

  const base::TimeTicks wait_for_vsync_start_time = base::TimeTicks::Now();
  bool wait_succeeded = WaitForVSyncImpl(&vsync_interval);
  const base::TimeDelta wait_for_vsync_elapsed_time =
      base::TimeTicks::Now() - wait_for_vsync_start_time;

  // WaitForVBlank and DCompositionWaitForCompositorClock returns very early
  // instead of waiting until vblank when the monitor goes to sleep or is
  // unplugged (nothing to present due to desktop occlusion). We use 1ms as
  // a threshhold for the duration of the wait functions and fallback to
  // Sleep() if it returns before that. This could happen during normal
  // operation for the first call after the vsync thread becomes non-idle,
  // but it shouldn't happen often.
  constexpr auto kVBlankIntervalThreshold = base::Milliseconds(1);
  if (!wait_succeeded ||
      wait_for_vsync_elapsed_time < kVBlankIntervalThreshold) {
    TRACE_EVENT0("gpu", "WaitForVSync Sleep");
    base::Time::ActivateHighResolutionTimer(true);
    Sleep(static_cast<DWORD>(vsync_interval.InMillisecondsRoundedUp()));
    base::Time::ActivateHighResolutionTimer(false);
  }

  AutoVSyncThreadLock auto_lock(this);
  DCHECK(is_vsync_task_posted_);
  is_vsync_task_posted_ = false;
  PostTaskIfNeeded();

  const base::TimeTicks vsync_time = base::TimeTicks::Now();
  observers_.Notify(&VSyncObserver::OnVSync, vsync_time, vsync_interval);
}

}  // namespace gl

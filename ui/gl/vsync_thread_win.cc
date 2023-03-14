// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/vsync_observer.h"

namespace gl {
namespace {
Microsoft::WRL::ComPtr<IDXGIOutput> DXGIOutputFromMonitor(
    HMONITOR monitor,
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return nullptr;
  }

  size_t i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, &output)))
      break;

    DXGI_OUTPUT_DESC desc = {};
    if (FAILED(output->GetDesc(&desc))) {
      DLOG(ERROR) << "DXGI output GetDesc failed";
      return nullptr;
    }

    if (desc.Monitor == monitor)
      return output;
  }

  return nullptr;
}
}  // namespace

// static
VSyncThreadWin* VSyncThreadWin::GetInstance() {
  return base::Singleton<VSyncThreadWin>::get();
}

VSyncThreadWin::VSyncThreadWin()
    : vsync_thread_("GpuVSyncThread"),
      vsync_provider_(gfx::kNullAcceleratedWidget),
      d3d11_device_(QueryD3D11DeviceObjectFromANGLE()) {
  DCHECK(d3d11_device_);

  is_suspended_ =
      base::PowerMonitor::AddPowerSuspendObserverAndReturnSuspendedState(this);

  vsync_thread_.StartWithOptions(
      base::Thread::Options(base::ThreadType::kDisplayCritical));
}

VSyncThreadWin::~VSyncThreadWin() {
  {
    base::AutoLock auto_lock(lock_);
    observers_.clear();
  }
  vsync_thread_.Stop();

  base::PowerMonitor::RemovePowerSuspendObserver(this);
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
  base::AutoLock auto_lock(lock_);
  observers_.insert(obs);
  PostTaskIfNeeded();
}

void VSyncThreadWin::RemoveObserver(VSyncObserver* obs) {
  base::AutoLock auto_lock(lock_);
  observers_.erase(obs);
}

void VSyncThreadWin::OnSuspend() {
  base::AutoLock auto_lock(lock_);
  is_suspended_ = true;
}

void VSyncThreadWin::OnResume() {
  base::AutoLock auto_lock(lock_);
  is_suspended_ = false;
  PostTaskIfNeeded();
}

void VSyncThreadWin::WaitForVSync() {
  base::TimeTicks vsync_phase;
  base::TimeDelta vsync_interval;
  const bool get_vsync_params_succeeded =
      vsync_provider_.GetVSyncParametersIfAvailable(&vsync_phase,
                                                    &vsync_interval);
  DCHECK(get_vsync_params_succeeded);

  // From Raymond Chen's blog "How do I get a handle to the primary monitor?"
  // https://devblogs.microsoft.com/oldnewthing/20141106-00/?p=43683
  const HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  if (primary_monitor_ != monitor) {
    primary_monitor_ = monitor;
    primary_output_ = DXGIOutputFromMonitor(monitor, d3d11_device_);
  }

  const base::TimeTicks wait_for_vblank_start_time = base::TimeTicks::Now();
  const bool wait_for_vblank_succeeded =
      primary_output_ && SUCCEEDED(primary_output_->WaitForVBlank());

  // WaitForVBlank returns very early instead of waiting until vblank when the
  // monitor goes to sleep.  We use 1ms as a threshold for the duration of
  // WaitForVBlank and fallback to Sleep() if it returns before that.  This
  // could happen during normal operation for the first call after the vsync
  // thread becomes non-idle, but it shouldn't happen often.
  constexpr auto kVBlankIntervalThreshold = base::Milliseconds(1);
  const base::TimeDelta wait_for_vblank_elapsed_time =
      base::TimeTicks::Now() - wait_for_vblank_start_time;
  if (!wait_for_vblank_succeeded ||
      wait_for_vblank_elapsed_time < kVBlankIntervalThreshold) {
    TRACE_EVENT("gpu", "WaitForVSync Sleep");
    base::Time::ActivateHighResolutionTimer(true);
    Sleep(static_cast<DWORD>(vsync_interval.InMillisecondsRoundedUp()));
    base::Time::ActivateHighResolutionTimer(false);
  }

  base::AutoLock auto_lock(lock_);
  DCHECK(is_vsync_task_posted_);
  is_vsync_task_posted_ = false;
  PostTaskIfNeeded();

  const base::TimeTicks vsync_time = base::TimeTicks::Now();
  for (auto* obs : observers_)
    obs->OnVSync(vsync_time, vsync_interval);
}

}  // namespace gl

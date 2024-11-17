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

namespace gl {
namespace {

// Whether the current thread holds the `VSyncThreadWin` lock.
thread_local bool g_current_thread_holds_lock = false;

// Check if a DXGI adapter is stale and needs to be replaced. This can happen
// e.g. when detaching/reattaching remote desktop sessions and causes subsequent
// WaitForVSyncs on the stale adapter/output to return instantly.
bool DXGIFactoryIsCurrent(IDXGIAdapter* dxgi_adapter) {
  CHECK(dxgi_adapter);

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  hr = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  CHECK_EQ(S_OK, hr);
  return dxgi_factory->IsCurrent();
}

// Create a new factory and find a DXGI adapter matching a LUID. This is useful
// if we have a previous adapter whose factory has become stale.
Microsoft::WRL::ComPtr<IDXGIAdapter> FindDXGIAdapterOnNewFactory(
    const LUID luid) {
  HRESULT hr = S_OK;

  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  CHECK_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<IDXGIAdapter1> new_adapter;
  for (uint32_t i = 0;; i++) {
    hr = factory->EnumAdapters1(i, &new_adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      DLOG(ERROR) << "EnumAdapters1 failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    DXGI_ADAPTER_DESC1 new_adapter_desc;
    hr = new_adapter->GetDesc1(&new_adapter_desc);
    CHECK_EQ(S_OK, hr);

    if (new_adapter_desc.AdapterLuid.HighPart == luid.HighPart &&
        new_adapter_desc.AdapterLuid.LowPart == luid.LowPart) {
      return new_adapter;
    }
  }

  DLOG(ERROR) << "Failed to find DXGI adapter with matching LUID";
  return nullptr;
}

// Return true if |output| is on |monitor|.
bool DXGIOutputIsOnMonitor(IDXGIOutput* output, const HMONITOR monitor) {
  CHECK(output);

  DXGI_OUTPUT_DESC desc = {};
  HRESULT hr = output->GetDesc(&desc);
  CHECK_EQ(S_OK, hr);
  return desc.Monitor == monitor;
}

Microsoft::WRL::ComPtr<IDXGIOutput> DXGIOutputFromMonitor(
    HMONITOR monitor,
    IDXGIAdapter* dxgi_adapter) {
  CHECK(dxgi_adapter);

  HRESULT hr = S_OK;

  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  for (uint32_t i = 0;; i++) {
    hr = dxgi_adapter->EnumOutputs(i, &output);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      DLOG(ERROR) << "EnumOutputs failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    if (DXGIOutputIsOnMonitor(output.Get(), monitor)) {
      return output;
    }
  }

  DLOG(ERROR) << "Failed to find DXGI output with matching monitor";
  return nullptr;
}

Microsoft::WRL::ComPtr<IDXGIAdapter> GetAdapter(IDXGIDevice* device) {
  CHECK(device);

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  CHECK_EQ(S_OK, device->GetAdapter(&adapter));
  return adapter;
}

LUID GetLuid(IDXGIAdapter* adapter) {
  CHECK(adapter);

  DXGI_ADAPTER_DESC desc;
  HRESULT hr = adapter->GetDesc(&desc);
  CHECK_EQ(S_OK, hr);
  return desc.AdapterLuid;
}
}  // namespace

// static
VSyncThreadWin* VSyncThreadWin::GetInstance() {
  static VSyncThreadWin* vsync_thread = []() -> VSyncThreadWin* {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device(
        GetDirectCompositionD3D11Device());
    if (!d3d11_device) {
      return nullptr;
    }
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);
    return new VSyncThreadWin(std::move(dxgi_device));
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

VSyncThreadWin::VSyncThreadWin(Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device)
    : vsync_thread_("GpuVSyncThread"),
      vsync_provider_(gfx::kNullAcceleratedWidget),
      dxgi_adapter_(GetAdapter(dxgi_device.Get())),
      original_adapter_luid_(GetLuid(dxgi_adapter_.Get())) {
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

base::TimeDelta VSyncThreadWin::GetVsyncInterval() {
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;

  // This is a simplified initial approach to fix crbug.com/1456399
  // In Windows SV3 builds DWM will operate with per monitor refresh
  // rates. As a result of this, DwmGetCompositionTimingInfo is no longer
  // guaranteed to align with the primary monitor but will instead align
  // with the current highest refresh rate monitor. This can cause issues
  // in clients which may be waiting on the primary monitor's vblank as
  // the reported interval may no longer match with the vblank wait.
  // To work around this discrepancy get the VSync interval directly from
  // monitor associated with window_ or the primary monitor.
  static bool use_sv3_workaround =
      base::win::GetVersion() > base::win::Version::WIN11_22H2 &&
      base::FeatureList::IsEnabled(
          features::kUsePrimaryMonitorVSyncIntervalOnSV3);

  const bool get_vsync_params_succeeded =
      use_sv3_workaround
          ? vsync_provider_.GetVSyncIntervalIfAvailable(&vsync_interval)
          : vsync_provider_.GetVSyncParametersIfAvailable(&vsync_timebase,
                                                          &vsync_interval);
  DCHECK(get_vsync_params_succeeded);
  return vsync_interval;
}

void VSyncThreadWin::WaitForVSync() {
  base::TimeDelta vsync_interval = GetVsyncInterval();

  if (!dxgi_adapter_ || !DXGIFactoryIsCurrent(dxgi_adapter_.Get())) {
    TRACE_EVENT("gpu", "DXGIFactoryIsCurrent non-current factory");
    dxgi_adapter_ = FindDXGIAdapterOnNewFactory(original_adapter_luid_);
    primary_output_.Reset();
  }

  // From Raymond Chen's blog "How do I get a handle to the primary monitor?"
  // https://devblogs.microsoft.com/oldnewthing/20141106-00/?p=43683
  const HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  if (primary_output_ &&
      !DXGIOutputIsOnMonitor(primary_output_.Get(), monitor)) {
    TRACE_EVENT("gpu", "DXGIOutputIsOnMonitor primary monitor changed");
    primary_output_.Reset();
  }

  if (!primary_output_ && dxgi_adapter_) {
    primary_output_ = DXGIOutputFromMonitor(monitor, dxgi_adapter_.Get());
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
    TRACE_EVENT2("gpu", "WaitForVSync Sleep", "has adapter", !!dxgi_adapter_,
                 "has output", !!primary_output_);
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

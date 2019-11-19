// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
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
      d3d11_device_(QueryD3D11DeviceObjectFromANGLE()) {
  DCHECK(d3d11_device_);
  base::Thread::Options options;
  options.priority = base::ThreadPriority::DISPLAY;
  vsync_thread_.StartWithOptions(std::move(options));
}

VSyncThreadWin::~VSyncThreadWin() {
  {
    base::AutoLock auto_lock(lock_);
    observers_.clear();
  }
  vsync_thread_.Stop();
}

void VSyncThreadWin::AddObserver(VSyncObserver* obs) {
  base::AutoLock auto_lock(lock_);
  observers_.insert(obs);
  if (is_idle_) {
    is_idle_ = false;
    vsync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VSyncThreadWin::WaitForVSync, base::Unretained(this)));
  }
}

void VSyncThreadWin::RemoveObserver(VSyncObserver* obs) {
  base::AutoLock auto_lock(lock_);
  observers_.erase(obs);
}

void VSyncThreadWin::WaitForVSync() {
  // From Raymond Chen's blog "How do I get a handle to the primary monitor?"
  // https://devblogs.microsoft.com/oldnewthing/20141106-00/?p=43683
  HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  if (primary_monitor_ != monitor) {
    primary_monitor_ = monitor;
    primary_output_ = DXGIOutputFromMonitor(monitor, d3d11_device_);
  }

  base::TimeDelta interval = base::TimeDelta::FromSecondsD(1.0 / 60);

  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(MONITORINFOEX);
  if (monitor && GetMonitorInfo(monitor, &monitor_info)) {
    DEVMODE display_info = {};
    display_info.dmSize = sizeof(DEVMODE);
    display_info.dmDriverExtra = 0;
    if (EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
                            &display_info) &&
        display_info.dmDisplayFrequency > 1) {
      interval =
          base::TimeDelta::FromSecondsD(1.0 / display_info.dmDisplayFrequency);
    }
  }

  base::TimeTicks wait_for_vblank_start_time = base::TimeTicks::Now();
  bool wait_for_vblank_succeeded =
      primary_output_ && SUCCEEDED(primary_output_->WaitForVBlank());

  // WaitForVBlank returns very early instead of waiting until vblank when the
  // monitor goes to sleep.  We use 1ms as a threshold for the duration of
  // WaitForVBlank and fallback to Sleep() if it returns before that.  This
  // could happen during normal operation for the first call after the vsync
  // thread becomes non-idle, but it shouldn't happen often.
  const auto kVBlankIntervalThreshold = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta wait_for_vblank_elapsed_time =
      base::TimeTicks::Now() - wait_for_vblank_start_time;

  if (!wait_for_vblank_succeeded ||
      wait_for_vblank_elapsed_time < kVBlankIntervalThreshold) {
    Sleep(static_cast<DWORD>(interval.InMillisecondsRoundedUp()));
  }

  base::AutoLock auto_lock(lock_);
  if (!observers_.empty()) {
    vsync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VSyncThreadWin::WaitForVSync, base::Unretained(this)));
    base::TimeTicks vsync_time = base::TimeTicks::Now();
    for (auto* obs : observers_)
      obs->OnVSync(vsync_time, interval);
  } else {
    is_idle_ = true;
  }
}

}  // namespace gl

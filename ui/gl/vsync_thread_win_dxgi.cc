// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/vsync_thread_win_dxgi.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"

namespace gl {
namespace {

// Check if a DXGI output's factory is stale and needs to be replaced. This can
// happen e.g. when detaching/reattaching remote desktop sessions and causes
// subsequent WaitForVSyncs on the stale output to return instantly.
bool DXGIFactoryIsCurrent(IDXGIOutput* dxgi_output) {
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  HRESULT hr = dxgi_output->GetParent(IID_PPV_ARGS(&dxgi_adapter));
  CHECK_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  hr = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  CHECK_EQ(S_OK, hr);
  return dxgi_factory->IsCurrent();
}

// Return true if |output| is on |monitor|.
bool DXGIOutputIsOnMonitor(IDXGIOutput* output, const HMONITOR monitor) {
  DXGI_OUTPUT_DESC desc = {};
  const HRESULT hr = output->GetDesc(&desc);
  CHECK_EQ(S_OK, hr);
  return desc.Monitor == monitor;
}

Microsoft::WRL::ComPtr<IDXGIOutput> DXGIOutputFromMonitor(
    HMONITOR monitor,
    IDXGIAdapter* dxgi_adapter) {
  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  for (uint32_t i = 0;; i++) {
    const HRESULT hr = dxgi_adapter->EnumOutputs(i, &output);
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

// Create a new factory and find a DXGI output on the target monitor by
// enumerating *all* adapters. The found adapter may be different than the
// rendering adapter if Chromium chooses an adapter which is not connected to
// any display: ex --force_high_performance_gpu.
Microsoft::WRL::ComPtr<IDXGIOutput> FindDXGIOutputForMonitor(HMONITOR monitor) {
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  CHECK_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  for (uint32_t i = 0;; i++) {
    hr = factory->EnumAdapters1(i, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      DLOG(ERROR) << "EnumAdapters1 failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    // Check if this adapter has an output on the target monitor.
    Microsoft::WRL::ComPtr<IDXGIOutput> output =
        DXGIOutputFromMonitor(monitor, adapter.Get());
    if (output) {
      return output;
    }
  }

  DLOG(ERROR) << "Failed to find DXGI output on monitor";
  return nullptr;
}

}  // namespace

VSyncThreadWinDXGI::VSyncThreadWinDXGI()
    : VSyncThreadWin(), vsync_provider_(gfx::kNullAcceleratedWidget) {}

VSyncThreadWinDXGI::~VSyncThreadWinDXGI() {
  NOTREACHED();
}

gfx::VSyncProvider* VSyncThreadWinDXGI::vsync_provider() {
  return &vsync_provider_;
}

base::TimeDelta VSyncThreadWinDXGI::GetVsyncInterval() {
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
      base::win::GetVersion() > base::win::Version::WIN11_22H2;

  const bool get_vsync_params_succeeded =
      use_sv3_workaround
          ? vsync_provider_.GetVSyncIntervalIfAvailable(&vsync_interval)
          : vsync_provider_.GetVSyncParametersIfAvailable(&vsync_timebase,
                                                          &vsync_interval);
  DCHECK(get_vsync_params_succeeded);

  return vsync_interval;
}

bool VSyncThreadWinDXGI::WaitForVSyncImpl(base::TimeDelta* vsync_interval) {
  *vsync_interval = GetVsyncInterval();

  // From Raymond Chen's blog "How do I get a handle to the primary monitor?"
  // https://devblogs.microsoft.com/oldnewthing/20141106-00/?p=43683
  const HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

  // If we have a cached primary output, determine whether we can still use it.
  if (primary_output_) {
    // Outputs from non-current adapters return error when you call
    // WaitForVBlank.
    if (!DXGIFactoryIsCurrent(primary_output_.Get())) {
      TRACE_EVENT("gpu", "DXGIFactoryIsCurrent non-current factory");
      primary_output_.Reset();
    } else if (!DXGIOutputIsOnMonitor(primary_output_.Get(), monitor)) {
      // Ensure that the output is still connected to the primary monitor.
      TRACE_EVENT("gpu", "DXGIOutputIsOnMonitor primary monitor changed");
      primary_output_.Reset();
    }
  }

  if (!primary_output_) {
    primary_output_ = FindDXGIOutputForMonitor(monitor);
  }

  // WaitForVBlank returns success on desktop occlusion and returns early
  const bool wait_succeeded =
      primary_output_ && SUCCEEDED(primary_output_->WaitForVBlank());
  if (!wait_succeeded) {
    TRACE_EVENT1("gpu", "WaitForVSyncImpl", "has output", !!primary_output_);
    return false;
  }

  return true;
}

}  // namespace gl

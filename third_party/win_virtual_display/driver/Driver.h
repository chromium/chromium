// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_

// Make sure we don't get min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <wdf.h>

#include <avrt.h>
#include <bugcodes.h>
#include <d3d11_2.h>
#include <dxgi1_5.h>
#include <iddcx.h>
#include <wrl.h>
#include <wudfwdm.h>

#include <memory>
#include <vector>

#include "Trace.h"

namespace Microsoft {
namespace WRL {
namespace Wrappers {
// Adds a wrapper for thread handles to the existing set of WRL handle wrapper
// classes
typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
}  // namespace Wrappers
}  // namespace WRL
}  // namespace Microsoft

namespace Microsoft {
namespace IndirectDisp {
/// <summary>
/// Manages the creation and lifetime of a Direct3D render device.
/// </summary>
struct IndirectSampleMonitor {
  static constexpr size_t szEdidBlock = 128;
  static constexpr size_t szModeList = 3;

  const BYTE pEdidBlock[szEdidBlock];
  const struct SampleMonitorMode {
    DWORD Width;
    DWORD Height;
    DWORD VSync;
  } pModeList[szModeList];
  const DWORD ulPreferredModeIdx;
};

/// <summary>
/// Manages the creation and lifetime of a Direct3D render device.
/// </summary>
struct Direct3DDevice {
  Direct3DDevice(LUID AdapterLuid);
  Direct3DDevice();
  HRESULT Init();

  LUID AdapterLuid;
  Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
  Microsoft::WRL::ComPtr<ID3D11Device> Device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};

/// <summary>
/// Manages a thread that consumes buffers from an indirect display swap-chain
/// object.
/// </summary>
class SwapChainProcessor {
 public:
  SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain,
                     std::unique_ptr<Direct3DDevice> Device,
                     HANDLE NewFrameEvent);
  ~SwapChainProcessor();

 private:
  static DWORD CALLBACK RunThread(LPVOID Argument);

  void Run();
  void RunCore();

  IDDCX_SWAPCHAIN m_hSwapChain;
  std::unique_ptr<Direct3DDevice> m_Device;
  HANDLE m_hAvailableBufferEvent;
  Microsoft::WRL::Wrappers::Thread m_hThread;
  Microsoft::WRL::Wrappers::Event m_hTerminateEvent;
};

/// <summary>
/// Provides a sample implementation of an indirect display driver.
/// </summary>
class IndirectDeviceContext {
 public:
  IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
  virtual ~IndirectDeviceContext();

  void InitAdapter();
  void FinishInit(UINT ConnectorIndex);

 protected:
  WDFDEVICE m_WdfDevice;
  IDDCX_ADAPTER m_Adapter;
};

class IndirectMonitorContext {
 public:
  IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor);
  virtual ~IndirectMonitorContext();

  void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain,
                       LUID RenderAdapter,
                       HANDLE NewFrameEvent);
  void UnassignSwapChain();

 private:
  IDDCX_MONITOR m_Monitor;
  std::unique_ptr<SwapChainProcessor> m_ProcessingThread;
};
}  // namespace IndirectDisp
}  // namespace Microsoft

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_

// Copyright (c) Microsoft Corporation

#include "SwapChainProcessor.h"

namespace display::test {

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain,
                                       std::unique_ptr<Direct3DDevice> Device,
                                       HANDLE NewFrameEvent)
    : m_hSwapChain(hSwapChain),
      m_Device(std::move(Device)),
      m_hAvailableBufferEvent(NewFrameEvent) {
  m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

  // Immediately create and run the swap-chain processing thread, passing 'this'
  // as the thread parameter
  m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor() {
  // Alert the swap-chain processing thread to terminate
  SetEvent(m_hTerminateEvent.Get());

  if (m_hThread.Get()) {
    // Wait for the thread to terminate
    WaitForSingleObject(m_hThread.Get(), INFINITE);
  }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument) {
  reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
  return 0;
}

void SwapChainProcessor::Run() {
  // For improved performance, make use of the Multimedia Class Scheduler
  // Service, which will intelligently prioritize this thread for improved
  // throughput in high CPU-load scenarios.
  DWORD AvTask = 0;
  HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);

  RunCore();

  // Always delete the swap-chain object when swap-chain processing loop
  // terminates in order to kick the system to provide a new swap-chain if
  // necessary.
  WdfObjectDelete((WDFOBJECT)m_hSwapChain);
  m_hSwapChain = nullptr;

  AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore() {
  // Get the DXGI device interface
  Microsoft::WRL::ComPtr<IDXGIDevice> DxgiDevice;
  HRESULT hr = m_Device->Device.As(&DxgiDevice);
  if (FAILED(hr)) {
    return;
  }

  IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
  SetDevice.pDevice = DxgiDevice.Get();

  hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
  if (FAILED(hr)) {
    return;
  }

  // Acquire and release buffers in a loop
  for (;;) {
    Microsoft::WRL::ComPtr<IDXGIResource> AcquiredBuffer;

    // Ask for the next buffer from the producer
    IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
    hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

    // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet
    // available
    if (hr == E_PENDING) {
      // We must wait for a new buffer
      HANDLE WaitHandles[] = {m_hAvailableBufferEvent, m_hTerminateEvent.Get()};
      DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles),
                                                WaitHandles, FALSE, 16);
      if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT) {
        // We have a new buffer, so try the AcquireBuffer again
        continue;
      } else if (WaitResult == WAIT_OBJECT_0 + 1) {
        // We need to terminate
        break;
      } else {
        // The wait was cancelled or something unexpected happened
        hr = HRESULT_FROM_WIN32(WaitResult);
        break;
      }
    } else if (SUCCEEDED(hr)) {
      // We have new frame to process, the surface has a reference on it that
      // the driver has to release
      AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

      // ==============================
      // TODO: Process the frame here
      //
      // This is the most performance-critical section of code in an IddCx
      // driver. It's important that whatever is done with the acquired surface
      // be finished as quickly as possible. This operation could be:
      //  * a GPU copy to another buffer surface for later processing (such as a
      //  staging surface for mapping to CPU memory)
      //  * a GPU encode operation
      //  * a GPU VPBlt to another surface
      //  * a GPU custom compute shader encode operation
      // ==============================

      // We have finished processing this frame hence we release the reference
      // on it. If the driver forgets to release the reference to the surface,
      // it will be leaked which results in the surfaces being left around after
      // swapchain is destroyed. NOTE: Although we release reference to the
      // surface here; the driver still owns the Buffer.MetaData.pSurface
      // surface until IddCxSwapChainReleaseAndAcquireBuffer returns S_OK and
      // gives us a new frame, a driver may want to use the surface in future
      // to re-encode the desktop for better quality if there is no new frame
      // for a while
      AcquiredBuffer.Reset();

      // Indicate to OS that we have finished inital processing of the frame, it
      // is a hint that OS could start preparing another frame
      hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
      if (FAILED(hr)) {
        break;
      }

      // ==============================
      // TODO: Report frame statistics once the asynchronous encode/send work is
      // completed
      //
      // Drivers should report information about sub-frame timings, like encode
      // time, send time, etc.
      // ==============================
      // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
    } else {
      // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so
      // exit the processing loop
      break;
    }
  }
}
}  // namespace display::test

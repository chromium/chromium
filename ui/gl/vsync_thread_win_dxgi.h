// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_DXGI_H_
#define UI_GL_VSYNC_THREAD_WIN_DXGI_H_

#include "ui/gl/vsync_thread_win.h"

namespace gl {
class GL_EXPORT VSyncThreadWinDXGI final : public VSyncThreadWin {
 public:
  explicit VSyncThreadWinDXGI(Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device);

  VSyncThreadWinDXGI(const VSyncThreadWinDXGI&) = delete;
  VSyncThreadWinDXGI& operator=(const VSyncThreadWinDXGI&) = delete;

  // Returns the vsync interval via the Vsync provider.
  base::TimeDelta GetVsyncInterval() final;

  gfx::VSyncProvider* vsync_provider() final;

 protected:
  bool WaitForVSyncImpl(base::TimeDelta* vsync_interval) final;

 private:
  ~VSyncThreadWinDXGI() final;

  // Used on vsync thread only after initialization.
  VSyncProviderWin vsync_provider_;

  // Used on vsync thread only after initialization
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
  Microsoft::WRL::ComPtr<IDXGIOutput> primary_output_;

  // The LUID of the adapter of the IDXGIDevice this instance was created with.
  const LUID original_adapter_luid_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_DXGI_H_

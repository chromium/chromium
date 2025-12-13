// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_DXGI_H_
#define UI_GL_VSYNC_THREAD_WIN_DXGI_H_

#include "ui/gl/vsync_provider_win.h"
#include "ui/gl/vsync_thread_win.h"

namespace gl {
class GL_EXPORT VSyncThreadWinDXGI final : public VSyncThreadWin {
 public:
  VSyncThreadWinDXGI();

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

  // Used on vsync thread only after initialization.
  Microsoft::WRL::ComPtr<IDXGIOutput> primary_output_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_DXGI_H_

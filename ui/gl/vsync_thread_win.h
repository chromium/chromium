// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_H_
#define UI_GL_VSYNC_THREAD_WIN_H_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/flat_set.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/vsync_provider_win.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace gl {
class VSyncObserver;
// Helper singleton that wraps a thread for calling IDXGIOutput::WaitForVBlank()
// for the primary monitor, and notifies observers on the same thread. Observers
// can be added or removed on the main thread, and the vsync thread goes to
// sleep if there are no observers. This is used by DirectCompositionSurfaceWin
// to plumb vsync signal back to the display compositor's BeginFrameSource.
class GL_EXPORT VSyncThreadWin final : public base::PowerSuspendObserver {
 public:
  static VSyncThreadWin* GetInstance();

  VSyncThreadWin(const VSyncThreadWin&) = delete;
  VSyncThreadWin& operator=(const VSyncThreadWin&) = delete;

  // Implementation of base::PowerSuspendObserver
  void OnSuspend() final;
  void OnResume() final;

  // These methods are not rentrancy safe, and shouldn't be called inside
  // VSyncObserver::OnVSync.  It's safe to assume that these can be called only
  // from the main thread.
  void AddObserver(VSyncObserver* obs);
  void RemoveObserver(VSyncObserver* obs);

  gfx::VSyncProvider* vsync_provider() { return &vsync_provider_; }

 private:
  friend struct base::DefaultSingletonTraits<VSyncThreadWin>;

  VSyncThreadWin();
  ~VSyncThreadWin() final;

  void PostTaskIfNeeded();
  void WaitForVSync();

  base::Thread vsync_thread_;

  // Used on vsync thread only after initialization.
  VSyncProviderWin vsync_provider_;
  const Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  HMONITOR primary_monitor_ = nullptr;
  Microsoft::WRL::ComPtr<IDXGIOutput> primary_output_;

  base::Lock lock_;
  bool GUARDED_BY(lock_) is_vsync_task_posted_ = false;
  bool GUARDED_BY(lock_) is_suspended_ = false;
  base::flat_set<VSyncObserver*> GUARDED_BY(lock_) observers_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_

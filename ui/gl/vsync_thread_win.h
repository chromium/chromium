// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_H_
#define UI_GL_VSYNC_THREAD_WIN_H_

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
// Helper singleton that wraps a thread for calling IDXGIOutput::WaitForVBlank()
// for the primary monitor, and notifies observers on the same thread. Observers
// can be added or removed on the main thread, and the vsync thread goes to
// sleep if there are no observers. This is used by ExternalBeginFrameSourceWin.
class GL_EXPORT VSyncThreadWin final : public base::PowerSuspendObserver {
 public:
  static VSyncThreadWin* GetInstance();

  VSyncThreadWin(const VSyncThreadWin&) = delete;
  VSyncThreadWin& operator=(const VSyncThreadWin&) = delete;

  // Implementation of base::PowerSuspendObserver
  void OnSuspend() final;
  void OnResume() final;

  class GL_EXPORT VSyncObserver {
   public:
    // Called on vsync thread.
    virtual void OnVSync(base::TimeTicks vsync_time,
                         base::TimeDelta interval) = 0;

   protected:
    virtual ~VSyncObserver() {}
  };
  // These methods are not rentrancy safe, and shouldn't be called inside
  // VSyncObserver::OnVSync.  It's safe to assume that these can be called only
  // from the main thread.
  void AddObserver(VSyncObserver* obs);
  void RemoveObserver(VSyncObserver* obs);

  gfx::VSyncProvider* vsync_provider() { return &vsync_provider_; }

 private:
  explicit VSyncThreadWin(Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device);
  ~VSyncThreadWin() final;

  void PostTaskIfNeeded();
  void WaitForVSync();

  base::Thread vsync_thread_;

  // Used on vsync thread only after initialization.
  VSyncProviderWin vsync_provider_;
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
  Microsoft::WRL::ComPtr<IDXGIOutput> primary_output_;

  // The LUID of the adapter of the IDXGIDevice this instance was created with.
  const LUID original_adapter_luid_;

  base::Lock lock_;
  bool GUARDED_BY(lock_) is_vsync_task_posted_ = false;
  bool GUARDED_BY(lock_) is_suspended_ = false;
  base::flat_set<raw_ptr<VSyncObserver, CtnExperimental>> GUARDED_BY(lock_)
      observers_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_

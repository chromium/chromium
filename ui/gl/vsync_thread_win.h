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
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
// Helper singleton that wraps a thread which calls IDXGIOutput::WaitForVBlank()
// for the primary monitor and notifies observers. Observers can be added or
// removed from any thread. The vsync thread sleeps when there are no observers.
// This is used by ExternalBeginFrameSourceWin.
class GL_EXPORT VSyncThreadWin final : public base::PowerSuspendObserver {
 public:
  static VSyncThreadWin* GetInstance();

  VSyncThreadWin(const VSyncThreadWin&) = delete;
  VSyncThreadWin& operator=(const VSyncThreadWin&) = delete;

  // Implementation of base::PowerSuspendObserver
  void OnSuspend() final;
  void OnResume() final;

  class GL_EXPORT VSyncObserver : public base::CheckedObserver {
   public:
    // Called on vsync thread.
    virtual void OnVSync(base::TimeTicks vsync_time,
                         base::TimeDelta interval) = 0;

   protected:
    ~VSyncObserver() override = default;
  };
  // These methods can be called from anywhere, including from inside an
  // OnVSync() notification.
  void AddObserver(VSyncObserver* obs);
  void RemoveObserver(VSyncObserver* obs);

  gfx::VSyncProvider* vsync_provider() { return &vsync_provider_; }

  // Returns the vsync interval via the Vsync provider.
  base::TimeDelta GetVsyncInterval();

 private:
  // Acquires `lock_` in a scope if not already held by the thread.
  class SCOPED_LOCKABLE AutoVSyncThreadLock;

  explicit VSyncThreadWin(Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device);
  ~VSyncThreadWin() final;

  void PostTaskIfNeeded() EXCLUSIVE_LOCKS_REQUIRED(lock_);
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
  base::ObserverList<VSyncObserver> GUARDED_BY(lock_) observers_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_

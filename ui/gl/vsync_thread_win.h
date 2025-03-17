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
class GL_EXPORT VSyncThreadWin : public base::PowerSuspendObserver {
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

  virtual gfx::VSyncProvider* vsync_provider() = 0;

  // Returns the vsync interval via the Vsync provider.
  virtual base::TimeDelta GetVsyncInterval() = 0;

 protected:
  VSyncThreadWin();
  ~VSyncThreadWin() override;

  // Gets vsync interval from vsync_provider and halts thread until the next
  // signal from the compositor clock or vblank. Returns true if the wait was
  // completed successfully, early if the desktop was occluded and false on any
  // other failures.
  virtual bool WaitForVSyncImpl(base::TimeDelta* vsync_interval) = 0;

 private:
  // Acquires `lock_` in a scope if not already held by the thread.
  class SCOPED_LOCKABLE AutoVSyncThreadLock;

  void WaitForVSync();

  void PostTaskIfNeeded() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::Thread vsync_thread_;

  base::Lock lock_;
  bool GUARDED_BY(lock_) is_vsync_task_posted_ = false;
  bool GUARDED_BY(lock_) is_suspended_ = false;
  base::ObserverList<VSyncObserver> GUARDED_BY(lock_) observers_;
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_

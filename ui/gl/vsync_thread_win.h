// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_H_
#define UI_GL_VSYNC_THREAD_WIN_H_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/flat_set.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"

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
class GL_EXPORT VSyncThreadWin {
 public:
  static VSyncThreadWin* GetInstance();

  // These methods are not rentrancy safe, and shouldn't be called inside
  // VSyncObserver::OnVSync.  It's safe to assume that these can be called only
  // from the main thread.
  void AddObserver(VSyncObserver* obs);
  void RemoveObserver(VSyncObserver* obs);

 private:
  friend struct base::DefaultSingletonTraits<VSyncThreadWin>;

  VSyncThreadWin();
  ~VSyncThreadWin();

  void WaitForVSync();

  base::Thread vsync_thread_;

  // Used on vsync thread only after initialization.
  const Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  HMONITOR primary_monitor_ = nullptr;
  Microsoft::WRL::ComPtr<IDXGIOutput> primary_output_;

  base::Lock lock_;
  bool GUARDED_BY(lock_) is_idle_ = true;
  base::flat_set<VSyncObserver*> GUARDED_BY(lock_) observers_;

  DISALLOW_COPY_AND_ASSIGN(VSyncThreadWin);
};
}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_

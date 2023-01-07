// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_
#define UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_

#include <windows.h>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "ui/gfx/gfx_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {

// This keeps track of whether a given HWND has a child window which the GPU
// process renders into. This should only be used from the UI thread unless
// otherwise noted.
class GFX_EXPORT RenderingWindowManager {
 public:
  // The first call to GetInstance() should happen on the UI thread.
  static RenderingWindowManager* GetInstance();

  RenderingWindowManager(const RenderingWindowManager&) = delete;
  RenderingWindowManager& operator=(const RenderingWindowManager&) = delete;

  void RegisterParent(HWND parent);
  // Registers |child| as child window for |parent|. Allows the GPU process to
  // draw into the |child| HWND instead of |parent|. This will fail and do
  // nothing if:
  //   1. |parent| isn't registered.
  //   2. |child| doesn't belong to |expected_child_process_id|.
  //
  // Can be called from any thread, as long GetInstance() has already been
  // called on the UI thread at least once.
  void RegisterChild(HWND parent, HWND child, DWORD expected_child_process_id);
  void UnregisterParent(HWND parent);
  bool HasValidChildWindow(HWND parent);

 private:
  friend class base::NoDestructor<RenderingWindowManager>;

  RenderingWindowManager();
  ~RenderingWindowManager();

  // UI thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Map from registered parent HWND to child HWND.
  base::flat_map<HWND, HWND> registered_hwnds_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_

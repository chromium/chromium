// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_CHILD_WINDOW_WIN_H_
#define UI_GL_CHILD_WINDOW_WIN_H_

#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"

#include <windows.h>

namespace gl {

// The window DirectComposition renders into needs to be owned by the process
// that's currently doing the rendering. The class creates and owns a window
// which is reparented by the browser to be a child of its window.
class GL_EXPORT ChildWindowWin {
 public:
  ChildWindowWin();
  ChildWindowWin(const ChildWindowWin&) = delete;
  ChildWindowWin& operator=(const ChildWindowWin&) = delete;

  ~ChildWindowWin();

  void Initialize();
  HWND window() const { return window_; }

  scoped_refptr<base::TaskRunner> GetTaskRunnerForTesting();

 private:
  // The window owner thread.
  std::unique_ptr<base::Thread> thread_;
  HWND window_ = nullptr;
  // The window is initially created with this parent window. We need to keep it
  // around so that we can destroy it at the end.
  HWND initial_parent_window_ = nullptr;
};

}  // namespace gl

#endif  // UI_GL_CHILD_WINDOW_WIN_H_

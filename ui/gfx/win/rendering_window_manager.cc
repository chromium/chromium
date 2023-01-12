// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/rendering_window_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"

namespace gfx {

// static
RenderingWindowManager* RenderingWindowManager::GetInstance() {
  static base::NoDestructor<RenderingWindowManager> instance;
  return instance.get();
}

void RenderingWindowManager::RegisterParent(HWND parent) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  registered_hwnds_.emplace(parent, nullptr);
}

void RenderingWindowManager::RegisterChild(HWND parent,
                                           HWND child,
                                           DWORD expected_child_process_id) {
  if (!child)
    return;

  // This can be called from any thread, if we're not on the correct thread then
  // PostTask back to the UI thread before doing anything.
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RenderingWindowManager::RegisterChild,
                                  base::Unretained(this), parent, child,
                                  expected_child_process_id));
    return;
  }

  // Check that |parent| was registered as a HWND that could have a child HWND.
  auto it = registered_hwnds_.find(parent);
  if (it == registered_hwnds_.end())
    return;

  // Check that |child| belongs to the GPU process.
  DWORD child_process_id = 0;
  DWORD child_thread_id = GetWindowThreadProcessId(child, &child_process_id);
  if (!child_thread_id || child_process_id != expected_child_process_id) {
    DLOG(ERROR) << "Child HWND not owned by GPU process.";
    return;
  }

  it->second = child;

  ::SetParent(child, parent);
  // Move D3D window behind Chrome's window to avoid losing some messages.
  ::SetWindowPos(child, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void RenderingWindowManager::UnregisterParent(HWND parent) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  registered_hwnds_.erase(parent);
}

bool RenderingWindowManager::HasValidChildWindow(HWND parent) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = registered_hwnds_.find(parent);
  if (it == registered_hwnds_.end())
    return false;
  return !!it->second && ::IsWindow(it->second);
}

RenderingWindowManager::RenderingWindowManager()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

RenderingWindowManager::~RenderingWindowManager() = default;

}  // namespace gfx

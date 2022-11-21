// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PROXY_HELPERS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PROXY_HELPERS_H_

#include <utility>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"

namespace ui {

// Posts a task to a different thread and blocks waiting for the task to finish
// executing.
void PostSyncTask(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::OnceCallback<void(base::WaitableEvent*)> callback);

// Creates a RepeatingCallback that will run |callback| on the calling thread.
// Useful when posting a task on a different thread and expecting a callback
// when the task finished (and the callback needs to run on the original
// thread).
template <typename... Args>
base::RepeatingCallback<void(Args...)> CreateSafeRepeatingCallback(
    base::RepeatingCallback<void(Args...)> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            std::move(callback), location);
}

// Creates a OnceCallback that will run |callback| on the calling thread. Useful
// when posting a task on a different thread and expecting a callback when the
// task finished (and the callback needs to run on the original thread).
template <typename... Args>
base::OnceCallback<void(Args...)> CreateSafeOnceCallback(
    base::OnceCallback<void(Args...)> callback,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            std::move(callback), location);
}

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PROXY_HELPERS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/jxl/v0_6/wrapper/parallel_runner.h"

#include <algorithm>
#include <atomic>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace blink::jxl_rs {

namespace {

void RunTasksUntilDone(std::atomic<size_t>* next_task_index,
                       size_t num_tasks,
                       void* context,
                       rust::Fn<void(void*, size_t)> task,
                       base::JobDelegate* delegate) {
  while (!delegate->ShouldYield()) {
    const size_t index =
        next_task_index->fetch_add(1, std::memory_order_relaxed);
    if (index >= num_tasks) {
      return;
    }
    task(context, index);
  }
}

size_t MaxConcurrency(std::atomic<size_t>* next_task_index,
                      size_t num_tasks,
                      size_t max_workers,
                      size_t /*worker_count*/) {
  const size_t started =
      std::min(next_task_index->load(std::memory_order_relaxed), num_tasks);
  return std::min(num_tasks - started, max_workers);
}

}  // namespace

void RunParallelTasks(size_t num_tasks,
                      void* context,
                      rust::Fn<void(void*, size_t)> task) {
  if (num_tasks <= 1 || !base::ThreadPoolInstance::Get()) {
    for (size_t index = 0; index < num_tasks; ++index) {
      task(context, index);
    }
    return;
  }

  // Every worker (and the joining thread) claims the next unclaimed task
  // index until all tasks have been claimed.
  std::atomic<size_t> next_task_index{0};
  const size_t max_workers =
      static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  base::CreateJob(FROM_HERE, {base::TaskPriority::USER_BLOCKING},
                base::BindRepeating(&RunTasksUntilDone,
                                    base::Unretained(&next_task_index),
                                    num_tasks, context, task),
                base::BindRepeating(&MaxConcurrency,
                                    base::Unretained(&next_task_index),
                                    num_tasks, max_workers))
      .Join();
}

}  // namespace blink::jxl_rs

/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "third_party/libjingle_xmpp/task_runner/taskrunner.h"

#include "base/logging.h"
#include "third_party/libjingle_xmpp/task_runner/task.h"

namespace jingle_xmpp {

TaskRunner::TaskRunner()
    : TaskParent(this) {}

TaskRunner::~TaskRunner() {
  // this kills and deletes children silently!
  AbortAllChildren();
  InternalRunTasks(true);
}

void TaskRunner::StartTask(Task * task) {
  tasks_.push_back(task);
  WakeTasks();
}

void TaskRunner::RunTasks() {
  InternalRunTasks(false);
}

void TaskRunner::InternalRunTasks(bool in_destructor) {
  // This shouldn't run while an abort is happening.
  // If that occurs, then tasks may be deleted in this method,
  // but pointers to them will still be in the
  // "ChildSet copy" in TaskParent::AbortAllChildren.
  // Subsequent use of those task may cause data corruption or crashes.
#if DCHECK_IS_ON
  DCHECK(!abort_count_);
#endif
  // Running continues until all tasks are Blocked (ok for a small # of tasks)
  if (tasks_running_) {
    return;  // don't reenter
  }

  tasks_running_ = true;

  int did_run = true;
  while (did_run) {
    did_run = false;
    // use indexing instead of iterators because tasks_ may grow
    for (size_t i = 0; i < tasks_.size(); ++i) {
      while (!tasks_[i]->Blocked()) {
        tasks_[i]->Step();
        did_run = true;
      }
    }
  }
  // Tasks are deleted when running has paused
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i]->IsDone()) {
      Task* task = tasks_[i];
#if DCHECK_IS_ON
      deleting_task_ = task;
#endif
      delete task;
#if DCHECK_IS_ON
      deleting_task_ = NULL;
#endif
      tasks_[i] = NULL;
    }
  }
  // Finally, remove nulls
  std::vector<Task *>::iterator it;
  it = std::remove(tasks_.begin(), tasks_.end(), static_cast<Task*>(NULL));

  tasks_.erase(it, tasks_.end());
  tasks_running_ = false;
}

} // namespace jingle_xmpp

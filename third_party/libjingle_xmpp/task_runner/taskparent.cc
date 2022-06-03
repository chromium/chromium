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

#include "third_party/libjingle_xmpp/task_runner/taskparent.h"

#include "base/check_op.h"
#include "third_party/libjingle_xmpp/task_runner/task.h"
#include "third_party/libjingle_xmpp/task_runner/taskrunner.h"

namespace jingle_xmpp {

TaskParent::TaskParent(Task* derived_instance, TaskParent *parent)
    : parent_(parent) {
  DCHECK(derived_instance != NULL);
  DCHECK(parent != NULL);
  runner_ = parent->GetRunner();
  parent_->AddChild(derived_instance);
  Initialize();
}

TaskParent::TaskParent(TaskRunner *derived_instance)
    : parent_(NULL),
      runner_(derived_instance) {
  DCHECK(derived_instance != NULL);
  Initialize();
}

TaskParent::~TaskParent() = default;

// Does common initialization of member variables
void TaskParent::Initialize() {
  children_.reset(new ChildSet());
  child_error_ = false;
}

void TaskParent::AddChild(Task *child) {
  children_->insert(child);
}

#if DCHECK_IS_ON
bool TaskParent::IsChildTask(Task *task) {
  DCHECK(task != NULL);
  return task->parent_ == this && children_->find(task) != children_->end();
}
#endif

bool TaskParent::AllChildrenDone() {
  for (ChildSet::iterator it = children_->begin();
       it != children_->end();
       ++it) {
    if (!(*it)->IsDone())
      return false;
  }
  return true;
}

bool TaskParent::AnyChildError() {
  return child_error_;
}

void TaskParent::AbortAllChildren() {
  if (children_->size() > 0) {
#if DCHECK_IS_ON
    runner_->IncrementAbortCount();
#endif

    ChildSet copy = *children_;
    for (ChildSet::iterator it = copy.begin(); it != copy.end(); ++it) {
      (*it)->Abort(true);  // Note we do not wake
    }

#if DCHECK_IS_ON
    runner_->DecrementAbortCount();
#endif
  }
}

void TaskParent::OnStopped(Task *task) {
  AbortAllChildren();
  parent_->OnChildStopped(task);
}

void TaskParent::OnChildStopped(Task *child) {
  if (child->HasError())
    child_error_ = true;
  children_->erase(child);
}

} // namespace jingle_xmpp

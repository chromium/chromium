// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_lock.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace ui {

CompositorLockManager::CompositorLockManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

CompositorLockManager::~CompositorLockManager() = default;

std::unique_ptr<CompositorLock> CompositorLockManager::GetCompositorLock(
    CompositorLockClient* client,
    base::TimeDelta timeout,
    base::OnceClosure release_callback) {
  // This uses the main WeakPtrFactory to break the connection from the lock to
  // the CompositorLockManager when the CompositorLockManager is destroyed.
  auto lock = std::make_unique<CompositorLock>(
      client, weak_ptr_factory_.GetWeakPtr(), std::move(release_callback));
  bool was_empty = active_locks_.empty();
  active_locks_.push_back(lock.get());

  bool should_extend_timeout = false;
  if ((was_empty || allow_locks_to_extend_timeout_) && !timeout.is_zero()) {
    const base::TimeTicks time_to_timeout = base::TimeTicks::Now() + timeout;
    // For the first lock, scheduled_timeout.is_null is true,
    // |time_to_timeout| will always larger than |scheduled_timeout_|. And it
    // is ok to invalidate the weakptr of |lock_timeout_weak_ptr_factory_|.
    if (time_to_timeout > scheduled_timeout_) {
      scheduled_timeout_ = time_to_timeout;
      should_extend_timeout = true;
      lock_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
    }
  }

  if (should_extend_timeout) {
    // The timeout task uses an independent WeakPtrFactory that is invalidated
    // when all locks are ended to prevent the timeout from leaking into
    // another lock that should have its own timeout.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CompositorLockManager::TimeoutLocks,
                       lock_timeout_weak_ptr_factory_.GetWeakPtr()),
        timeout);
  }
  return lock;
}

void CompositorLockManager::RemoveCompositorLock(CompositorLock* lock) {
  std::erase(active_locks_, lock);
  if (active_locks_.empty()) {
    lock_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
    scheduled_timeout_ = base::TimeTicks();
  }
}

void CompositorLockManager::TimeoutLocks() {
  // Make a copy, we're going to cause |active_locks_| to become empty.
  std::vector<raw_ptr<CompositorLock, VectorExperimental>> locks =
      active_locks_;
  for (ui::CompositorLock* lock : locks) {
    lock->TimeoutLock();
  }
  DCHECK(active_locks_.empty());
}

CompositorLock::CompositorLock(CompositorLockClient* client,
                               base::WeakPtr<CompositorLockManager> manager,
                               base::OnceClosure release_callback)
    : client_(client),
      release_callback_(std::move(release_callback)),
      manager_(std::move(manager)) {}

CompositorLock::~CompositorLock() {
  if (release_callback_) {
    std::move(release_callback_).Run();
  }
  if (manager_) {
    manager_->RemoveCompositorLock(this);
  }
}

void CompositorLock::TimeoutLock() {
  if (release_callback_) {
    std::move(release_callback_).Run();
  }
  manager_->RemoveCompositorLock(this);
  manager_ = nullptr;
  if (client_)
    client_->CompositorLockTimedOut();
}

}  // namespace ui

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_LOCK_H_
#define UI_COMPOSITOR_COMPOSITOR_LOCK_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class ScopedDeferMainFrameUpdate;
}

namespace ui {

class Compositor;
class CompositorLock;

// Implemented by clients which take compositor lock. Used to notify the client
// when their lock times out.
class CompositorLockClient {
 public:
  virtual ~CompositorLockClient() {}

  // Called if the CompositorLock ends before being destroyed due to timeout.
  virtual void CompositorLockTimedOut() = 0;
};

// A helper class used to manage compositor locks. Should be created/used by
// classes which want to provide out compositor locking.
class COMPOSITOR_EXPORT CompositorLockManager {
 public:
  CompositorLockManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~CompositorLockManager();

  // Creates a compositor lock. Returns NULL if it is not possible to lock at
  // this time (i.e. we're waiting to complete a previous unlock). If the
  // timeout is null, then no timeout is used.
  std::unique_ptr<CompositorLock> GetCompositorLock(
      CompositorLockClient* client,
      base::TimeDelta timeout,
      std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
          scoped_defer_main_frame_update);

  void set_allow_locks_to_extend_timeout(bool allowed) {
    allow_locks_to_extend_timeout_ = allowed;
  }

  bool IsLocked() const { return !active_locks_.empty(); }

  void TimeoutLocksForTesting() { TimeoutLocks(); }

 private:
  friend class CompositorLock;

  // Causes all active CompositorLocks to be timed out.
  void TimeoutLocks();

  // Called to perform the unlock operation.
  void RemoveCompositorLock(CompositorLock*);

  // The TaskRunner on which timeouts are run.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // The estimated time that the locks will timeout.
  base::TimeTicks scheduled_timeout_;
  // If true, the |scheduled_timeout_| might be recalculated and extended.
  bool allow_locks_to_extend_timeout_ = false;
  // The set of locks that are held externally.
  std::vector<CompositorLock*> active_locks_;

  base::WeakPtrFactory<CompositorLockManager> weak_ptr_factory_{this};
  base::WeakPtrFactory<CompositorLockManager> lock_timeout_weak_ptr_factory_{
      this};
};

// This class represents a lock on the compositor, that can be used to prevent
// commits to the compositor tree while we're waiting for an asynchronous
// event. The typical use case is when waiting for a renderer to produce a frame
// at the right size. The caller keeps a reference on this object, and drops the
// reference once it desires to release the lock.
// By default, the lock will be cancelled after a short timeout to ensure
// responsiveness of the UI, so the compositor tree should be kept in a
// "reasonable" state while the lock is held. The timeout length, or no timeout,
// can be requested at the time the lock is created.
// Don't instantiate this class directly, use Compositor::GetCompositorLock.
class COMPOSITOR_EXPORT CompositorLock {
 public:
  // The |client| is informed about events from the CompositorLock. The
  // |delegate| is used to perform actual unlocking. If |timeout| is zero then
  // no timeout is scheduled, else a timeout is scheduled on the |task_runner|.
  explicit CompositorLock(CompositorLockClient* client,
                          base::WeakPtr<CompositorLockManager> manager,
                          std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
                              scoped_defer_main_frame_update);
  ~CompositorLock();

 private:
  friend class CompositorLockManager;

  // Causes the CompositorLock to end due to a timeout.
  void TimeoutLock();

  CompositorLockClient* const client_;
  std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
      scoped_defer_main_frame_update_;
  base::WeakPtr<CompositorLockManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(CompositorLock);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_LOCK_H_

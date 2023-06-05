// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor_lock.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

// For tests that control time.
class CompositorLockTest : public testing::Test {
 protected:
  CompositorLockTest() {}
  ~CompositorLockTest() override {}

  void SetUp() override {
    task_runner_ = new base::TestMockTimeTaskRunner;
    lock_manager_ = std::make_unique<CompositorLockManager>(task_runner_);
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  CompositorLockManager* lock_manager() { return lock_manager_.get(); }

  void DestroyLockManager() { lock_manager_.reset(); }

  base::OnceClosure CreateReleaseCallback() {
    return base::BindOnce(
        [](CompositorLockTest* self) { self->num_releases_++; },
        base::Unretained(this));
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<CompositorLockManager> lock_manager_;
  bool is_locked_ = false;
  uint32_t num_releases_ = 0u;
};

class MockCompositorLockClient : public ui::CompositorLockClient {
 public:
  MOCK_METHOD0(CompositorLockTimedOut, void());
};

}  // namespace

TEST_F(CompositorLockTest, LocksTimeOut) {
  base::TimeDelta timeout = base::Milliseconds(100);

  {
    testing::StrictMock<MockCompositorLockClient> lock_client;
    // This lock has a timeout.
    std::unique_ptr<CompositorLock> lock = lock_manager()->GetCompositorLock(
        &lock_client, timeout, CreateReleaseCallback());
    EXPECT_TRUE(lock_manager()->IsLocked());
    EXPECT_CALL(lock_client, CompositorLockTimedOut()).Times(1);
    task_runner()->FastForwardBy(timeout);
    task_runner()->RunUntilIdle();
    EXPECT_FALSE(lock_manager()->IsLocked());
    EXPECT_EQ(1u, num_releases_);
  }

  {
    testing::StrictMock<MockCompositorLockClient> lock_client;
    // This lock has no timeout.
    std::unique_ptr<CompositorLock> lock = lock_manager()->GetCompositorLock(
        &lock_client, base::TimeDelta(), CreateReleaseCallback());
    EXPECT_TRUE(lock_manager()->IsLocked());
    EXPECT_CALL(lock_client, CompositorLockTimedOut()).Times(0);
    task_runner()->FastForwardBy(timeout);
    task_runner()->RunUntilIdle();
    EXPECT_TRUE(lock_manager()->IsLocked());
    EXPECT_EQ(1u, num_releases_);
  }
}

TEST_F(CompositorLockTest, MultipleLockClients) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout = base::Milliseconds(1);
  // Both locks are grabbed from the Compositor with a separate client.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout,
                                            CreateReleaseCallback());
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());
  // Both clients get notified of timeout.
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, ExtendingLifeOfLockDoesntUseDeadClient) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout = base::Milliseconds(1);

  // One lock is grabbed from the compositor with a client. The other
  // extends its lifetime past that of the first.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  // This also locks the compositor and will do so past |lock1| ending.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout,
                                            CreateReleaseCallback());
  // |lock1| is destroyed, so it won't timeout but |lock2| will.
  lock1 = nullptr;
  EXPECT_EQ(1u, num_releases_);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();

  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, AddingLocksDoesNotExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(1);
  base::TimeDelta timeout2 = base::Milliseconds(10);

  // The first lock has a short timeout.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  // The second lock has a longer timeout, but since a lock is active,
  // the first one is used for both.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, AllowAndExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(1);
  base::TimeDelta timeout2 = base::Milliseconds(10);

  // The first lock has a short timeout.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  // Allow locks to extend timeout.
  lock_manager()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a longer timeout, so the second one is used for both.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());
  lock_manager()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(lock_manager()->IsLocked());
  EXPECT_EQ(0u, num_releases_);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2 - timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, ExtendingTimeoutStartingCreatedTime) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(5);
  base::TimeDelta timeout2 = base::Milliseconds(10);

  // The first lock has a short timeout.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  base::TimeDelta time_elapse = base::Milliseconds(1);
  task_runner()->FastForwardBy(time_elapse);
  task_runner()->RunUntilIdle();

  // Allow locks to extend timeout.
  lock_manager()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a longer timeout, so the second one is used for both
  // and start from the time second lock created.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());
  lock_manager()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1 - time_elapse);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(lock_manager()->IsLocked());
  EXPECT_EQ(0u, num_releases_);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2 - (timeout1 - time_elapse));
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, AllowButNotExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(10);
  base::TimeDelta timeout2 = base::Milliseconds(1);

  // The first lock has a longer timeout.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  // Allow locks to extend timeout.
  lock_manager()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a short timeout, so the first one is used for both.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());
  lock_manager()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(lock_manager()->IsLocked());
  EXPECT_EQ(0u, num_releases_);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout1 - timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, AllowingExtendDoesNotUseDeadClient) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(1);
  base::TimeDelta timeout2 = base::Milliseconds(10);

  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(1u, num_releases_);

  // Allow locks to extend timeout.
  lock_manager()->set_allow_locks_to_extend_timeout(true);
  // |lock1| is timed out already. The second lock can timeout on its own.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());
  lock_manager()->set_allow_locks_to_extend_timeout(false);
  EXPECT_TRUE(lock_manager()->IsLocked());
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, LockIsDestroyedDoesntTimeout) {
  base::TimeDelta timeout = base::Milliseconds(1);

  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());
  // The CompositorLockClient is destroyed when |lock1| is released.
  lock1 = nullptr;
  EXPECT_EQ(1u, num_releases_);
  // The client isn't called as a result.
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(lock_manager()->IsLocked());
}

TEST_F(CompositorLockTest, TimeoutEndsWhenLockEnds) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::Milliseconds(1);
  base::TimeDelta timeout2 = base::Milliseconds(10);

  // The first lock has a short timeout.
  lock1 = lock_manager()->GetCompositorLock(&lock_client1, timeout1,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());
  // But the first lock is ended before timeout.
  lock1 = nullptr;
  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(1u, num_releases_);

  // The second lock has a longer timeout, and it should use that timeout,
  // since the first lock is done.
  lock2 = lock_manager()->GetCompositorLock(&lock_client2, timeout2,
                                            CreateReleaseCallback());
  EXPECT_TRUE(lock_manager()->IsLocked());

  {
    // The second lock doesn't timeout from the first lock which has ended.
    EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
    task_runner()->FastForwardBy(timeout1);
    task_runner()->RunUntilIdle();
  }

  {
    // The second lock can still timeout on its own though.
    EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
    task_runner()->FastForwardBy(timeout2 - timeout1);
    task_runner()->RunUntilIdle();
  }

  EXPECT_FALSE(lock_manager()->IsLocked());
  EXPECT_EQ(2u, num_releases_);
}

TEST_F(CompositorLockTest, CompositorLockOutlivesManager) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;

  lock1 = lock_manager()->GetCompositorLock(&lock_client1, base::TimeDelta(),
                                            CreateReleaseCallback());
  // The compositor is destroyed before the lock.
  DestroyLockManager();
  // This doesn't crash.
  lock1 = nullptr;
  EXPECT_EQ(1u, num_releases_);
}

}  // namespace ui

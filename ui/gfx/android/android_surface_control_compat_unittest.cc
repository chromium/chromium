// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/android_surface_control_compat.h"

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

class SurfaceControlTransactionTest : public testing::Test {
 public:
  SurfaceControlTransactionTest() {
    gfx::SurfaceControl::SetStubImplementationForTesting();
  }

 protected:
  struct CallbackContext {
    CallbackContext(bool* called, bool* destroyed)
        : called(called), destroyed(destroyed) {}
    ~CallbackContext() { *destroyed = true; }
    raw_ptr<bool> called;
    raw_ptr<bool> destroyed;
  };

  SurfaceControl::Transaction::OnCompleteCb CreateOnCompleteCb(
      bool* called,
      bool* destroyed) {
    return base::BindOnce(
        [](std::unique_ptr<CallbackContext> context,
           SurfaceControl::TransactionStats stats) {
          DCHECK(!*context->called);
          *context->called = true;
        },
        std::make_unique<CallbackContext>(called, destroyed));
  }

  SurfaceControl::Transaction::OnCommitCb CreateOnCommitCb(bool* called,
                                                           bool* destroyed) {
    return base::BindOnce(
        [](std::unique_ptr<CallbackContext> context) {
          DCHECK(!*context->called);
          *context->called = true;
        },
        std::make_unique<CallbackContext>(called, destroyed));
  }

  void RunRemainingTasks() {
    base::RunLoop runloop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, runloop.QuitClosure());
    runloop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(SurfaceControlTransactionTest, CallbackCalledAfterApply) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetOnCompleteCb(
      CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  transaction.SetOnCommitCb(
      CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Nothing should have been called yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  transaction.Apply();
  RunRemainingTasks();

  // After apply callbacks should be called.
  EXPECT_TRUE(on_complete_called);
  EXPECT_TRUE(on_commit_called);

  // As this is Once callback naturally it's context should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

TEST_F(SurfaceControlTransactionTest, CallbackDestroyedWithoutApply) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  {
    SurfaceControl::Transaction transaction;
    transaction.SetOnCompleteCb(
        CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    transaction.SetOnCommitCb(
        CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Nothing should have been called yet.
    EXPECT_FALSE(on_complete_called);
    EXPECT_FALSE(on_commit_called);
  }

  RunRemainingTasks();

  // Apply wasn't called, but transaction left the scope, so the callback
  // contexts should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

TEST_F(SurfaceControlTransactionTest, CallbackSetupAfterGetTransaction) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetOnCompleteCb(
      CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  transaction.SetOnCommitCb(
      CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Nothing should have been called yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  auto* asurfacetransaction = transaction.GetTransaction();

  // Should be no task to run, but calling this to make sure nothing is
  // scheduled that can call callbacks.
  RunRemainingTasks();

  // And not yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  // This is usually called by framework.
  SurfaceControl::ApplyTransaction(asurfacetransaction);
  RunRemainingTasks();

  // After apply callbacks should be called.
  EXPECT_TRUE(on_complete_called);
  EXPECT_TRUE(on_commit_called);

  // As this is Once callback naturally it's context should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

}  // namespace
}  // namespace gfx

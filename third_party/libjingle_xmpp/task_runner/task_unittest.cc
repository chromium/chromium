/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/task_runner/task.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/task_runner/taskrunner.h"

namespace jingle_xmpp {

class FakeTask : public Task {
 public:
  explicit FakeTask(TaskParent *parent) : Task(parent) {}
  int ProcessStart() override {
    return STATE_RESPONSE;
  }
};

// simple implementation of a task runner which uses Windows'
// GetSystemTimeAsFileTime() to get the current clock ticks
class MyTaskRunner : public TaskRunner {
 public:
  virtual void WakeTasks() { RunTasks(); }

  bool timeout_change() const {
    return timeout_change_;
  }

  void clear_timeout_change() {
    timeout_change_ = false;
  }
 protected:
  virtual void OnTimeoutChange() {
    timeout_change_ = true;
  }
  bool timeout_change_;
};

// Test for aborting the task while it is running

class AbortTask : public Task {
 public:
  explicit AbortTask(TaskParent *parent) : Task(parent) {
  }

  AbortTask(const AbortTask&) = delete;
  AbortTask& operator=(const AbortTask&) = delete;

  virtual int ProcessStart() {
    Abort();
    return STATE_NEXT;
  }
};

class TaskAbortTest : public sigslot::has_slots<> {
 public:
  TaskAbortTest() {}

  TaskAbortTest(const TaskAbortTest&) = delete;
  TaskAbortTest& operator=(const TaskAbortTest&) = delete;

  // no need to delete any tasks; the task runner owns them
  ~TaskAbortTest() {}

  void Start() {
    Task *abort_task = new AbortTask(&task_runner_);
    abort_task->Start();

    // run the task
    task_runner_.RunTasks();
  }

 private:
  void OnTimeout() {
    FAIL() << "Task timed out instead of aborting.";
  }

  MyTaskRunner task_runner_;
};

TEST(start_task_test, Abort) {
  TaskAbortTest abort_test;
  abort_test.Start();
}

// Test for aborting a task to verify that it does the Wake operation
// which gets it deleted.

class SetBoolOnDeleteTask : public Task {
 public:
  SetBoolOnDeleteTask(TaskParent *parent, bool *set_when_deleted)
    : Task(parent),
      set_when_deleted_(set_when_deleted) {
    EXPECT_TRUE(NULL != set_when_deleted);
    EXPECT_FALSE(*set_when_deleted);
  }

  SetBoolOnDeleteTask(const SetBoolOnDeleteTask&) = delete;
  SetBoolOnDeleteTask& operator=(const SetBoolOnDeleteTask&) = delete;

  virtual ~SetBoolOnDeleteTask() {
    *set_when_deleted_ = true;
  }

  virtual int ProcessStart() {
    return STATE_BLOCKED;
  }

 private:
  bool* set_when_deleted_;
};

class AbortShouldWakeTest : public sigslot::has_slots<> {
 public:
  AbortShouldWakeTest() {}

  AbortShouldWakeTest(const AbortShouldWakeTest&) = delete;
  AbortShouldWakeTest& operator=(const AbortShouldWakeTest&) = delete;

  // no need to delete any tasks; the task runner owns them
  ~AbortShouldWakeTest() {}

  void Start() {
    bool task_deleted = false;
    Task *task_to_abort = new SetBoolOnDeleteTask(&task_runner_, &task_deleted);
    task_to_abort->Start();

    // Task::Abort() should call TaskRunner::WakeTasks(). WakeTasks calls
    // TaskRunner::RunTasks() immediately which should delete the task.
    task_to_abort->Abort();
    EXPECT_TRUE(task_deleted);

    if (!task_deleted) {
      // avoid a crash (due to referencing a local variable)
      // if the test fails.
      task_runner_.RunTasks();
    }
  }

 private:
  void OnTimeout() {
    FAIL() << "Task timed out instead of aborting.";
  }

  MyTaskRunner task_runner_;
};

TEST(start_task_test, AbortShouldWake) {
  AbortShouldWakeTest abort_should_wake_test;
  abort_should_wake_test.Start();
}

class DeleteTestTaskRunner : public TaskRunner {
 public:
  DeleteTestTaskRunner() {
  }

  DeleteTestTaskRunner(const DeleteTestTaskRunner&) = delete;
  DeleteTestTaskRunner& operator=(const DeleteTestTaskRunner&) = delete;

  virtual void WakeTasks() { }
};

TEST(unstarted_task_test, DeleteTask) {
  // This test ensures that we don't
  // crash if a task is deleted without running it.
  DeleteTestTaskRunner task_runner;
  FakeTask* task = new FakeTask(&task_runner);
  task->Start();

  // try deleting the task directly
  FakeTask* child_task = new FakeTask(task);
  delete child_task;

  // run the unblocked tasks
  task_runner.RunTasks();
}

TEST(unstarted_task_test, DoNotDeleteTask1) {
  // This test ensures that we don't
  // crash if a task runner is deleted without
  // running a certain task.
  DeleteTestTaskRunner task_runner;
  FakeTask* task = new FakeTask(&task_runner);
  task->Start();

  FakeTask* child_task = new FakeTask(task);
  child_task->Start();

  // Never run the tasks
}

}  // namespace jingle_xmpp

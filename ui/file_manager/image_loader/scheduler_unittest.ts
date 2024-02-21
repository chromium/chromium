// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {ImageRequestTask} from './image_request_task.js';
import {MAXIMUM_IN_PARALLEL, Scheduler} from './scheduler.js';


/**
 * Fake global clock used to record the "time" at which a task was run.
 */
let globalTime = 0;

export function setUp() {
  globalTime = 0;
}

interface FakeImageRequestTask {
  cancelCallCount: number;
  runTime: number;
  getId(): string;
  getPriority(): number;
  cancel(): void;
  loadFromCacheAndProcess(onSuccess: VoidCallback, onFailure: VoidCallback):
      void;
}

function newTask(taskId: string, priority: number): FakeImageRequestTask {
  return {
    // Counts how many times cancel method was called.
    // Used to test multiple cancellation of the same task.
    cancelCallCount: 0,

    // Records value of globalTime variable at the time the main method,
    // loadFromCacheAndProcess is called. Used to test if the task was
    // executed and in what orders tasks were executed.
    runTime: 0,

    getId(): string {
      return taskId;
    },

    getPriority(): number {
      return priority;
    },

    cancel() {
      ++this.cancelCallCount;
    },

    loadFromCacheAndProcess(resolve: VoidCallback, _reject: VoidCallback) {
      this.runTime = ++globalTime;
      setTimeout(resolve);
    },
  };
}

/**
 * Checks that adding and removing tasks before the scheduler is started works.
 */
export function testIdleSchedulerAddRemove() {
  const scheduler = new Scheduler();
  const fakeTask = newTask('task-1', 0);
  scheduler.add(fakeTask as unknown as ImageRequestTask);
  assertEquals(0, fakeTask.cancelCallCount);
  scheduler.remove('task-1');
  assertEquals(1, fakeTask.cancelCallCount);
  scheduler.remove('task-1');
  assertEquals(1, fakeTask.cancelCallCount);
}

/**
 * Checks that tasks that were in newTasks are correctly copied to pending
 * tasks when scheduler is started. They also should be executed in the
 * order of their priorities.
 */
export function testNewTasksMovedAndRunInPriorityOrder() {
  const fakeTask1 = newTask('task-1', 1);
  const fakeTask2 = newTask('task-2', 0);

  const scheduler = new Scheduler();
  scheduler.add(fakeTask1 as unknown as ImageRequestTask);
  scheduler.add(fakeTask2 as unknown as ImageRequestTask);

  scheduler.start();
  assertEquals(2, fakeTask1.runTime);
  assertEquals(1, fakeTask2.runTime);
}

/**
 * Checks that the scheduler only launches MAXIMUM_IN_PARALLEL tasks.
 */
export function testParallelTasks() {
  const scheduler = new Scheduler();
  const taskList = [];
  for (let i = 0; i <= MAXIMUM_IN_PARALLEL; ++i) {
    taskList.push(newTask(`task-${i}`, 0));
    scheduler.add(taskList[i] as unknown as ImageRequestTask);
  }
  scheduler.start();
  for (let i = 0; i < MAXIMUM_IN_PARALLEL; ++i) {
    assertEquals(i + 1, taskList[i]!.runTime, `task ${i} did not run`);
  }
  assertEquals(0, taskList[MAXIMUM_IN_PARALLEL]!.runTime);
}

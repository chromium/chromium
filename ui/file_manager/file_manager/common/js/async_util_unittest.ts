// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {AsyncQueue, RateLimiter, type Task} from './async_util.js';
import {waitUntil} from './test_error_reporting.js';

/** Checks that the order of the tasks is preserved. */
export async function testAsyncQueueOrder() {
  const queue = new AsyncQueue();
  const taskTrace: string[] = [];

  const secondTask = (callback: VoidCallback) => {
    taskTrace.push('2');
    callback();
  };

  const firstTask = (callback: VoidCallback) => {
    queue.run(secondTask);
    setTimeout(() => {
      taskTrace.push('1');
      callback();
    }, 100);
  };

  queue.run(firstTask);
  await waitUntil(() => taskTrace.length === 2);
  assertArrayEquals(['1', '2'], taskTrace);
}

/** Checks that tasks with errors do not interrupt the queue's operations. */
export async function testAsyncQueueFailingTask() {
  const queue = new AsyncQueue();
  const taskTrace: string[] = [];

  const followingTask = (callback: VoidCallback) => {
    taskTrace.push('following');
    callback();
  };

  const badTask = (_callback: VoidCallback) => {
    taskTrace.push('bad');
    queue.run(followingTask);
    throw new Error('Something went wrong');
  };

  queue.run(badTask);
  await waitUntil(() => taskTrace.length === 2);
  assertArrayEquals(['bad', 'following'], taskTrace);
}

/**
 * Checks the order of start and end of two mutually calling tasks. We expect
 * that task that is run first finishes before the task ran after it.
 */
export async function testAsyncQueueStartEndOrder() {
  const queue = new AsyncQueue();
  let runCount = 0;
  const task: Task[] = [];
  const taskTrace: number[] = [];
  const maxRunCount = 4;

  // Makes a task that enqueues the task specified by the |index|.
  // Each task also records its trace right at the start and at the end.
  const makeTask = (index: number) => {
    return (callback: VoidCallback) => {
      const myID = runCount++;
      taskTrace.push(myID);
      if (runCount < maxRunCount) {
        queue.run(task[index]!);
      }
      callback();
      taskTrace.push(myID);
    };
  };

  // Task 0 enqueues task 1.
  task.push(makeTask(1));
  // Task 1 enqueues task 0.
  task.push(makeTask(0));
  // Kick off the process by running task 0.
  queue.run(task[0]!);

  await waitUntil(() => runCount >= maxRunCount);
  // TODO(1350885): This records problematic behavior in the test. Fixing
  // concurrent queue should change this tests to expect the sequence to be
  // [0, 0, 1, 1, 2, 2, 3, 3].
  const expected = [0, 1, 2, 3, 3, 2, 1, 0];
  assertArrayEquals(expected, taskTrace);
}

/**
 * Checks that calling `run` inside a RateRimiter's clojure does not lead to
 * recursive calls.
 */
export function testRateLimiterDoesNotLeadToRescursion(done: VoidCallback) {
  const limiter = new RateLimiter(() => {
    // This should not lead to an infinite recursion.
    limiter.run();
    done();
  });

  limiter.run();
}

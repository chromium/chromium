// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {importer} from '../../common/js/importer_common.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {taskQueueInterfaces} from '../../externs/background/task_queue.js';

import {taskQueue} from './task_queue.js';

/** @type {!taskQueueInterfaces.TaskQueue} */
let queue;

/** @type {!Object<importer.UpdateType, number>} */
const updates = {};

export function setUp() {
  queue = new taskQueue.TaskQueueImpl();

  // Set up a callback to log updates from running tasks.
  for (const updateType in importer.UpdateType) {
    // Reset counts for all update types.
    updates[importer.UpdateType[updateType]] = 0;
  }

  // Counts the number of updates of each type that have been received.
  const updateCallback = (type, updatedTask) => {
    updates[type]++;
  };
  queue.addUpdateCallback(updateCallback);
}

/**
 * A Task subclass for testing.
 */
class TestTask extends taskQueue.BaseTaskImpl {
  /**
   * @param {string} taskId
   */
  constructor(taskId) {
    super(taskId);

    /** @type {boolean} */
    this.wasRun = false;

    /**
     * @private {Function}
     */
    this.runResolver_ = null;

    this.runPromise_ = new Promise(resolve => {
      this.runResolver_ = resolve;
    });
  }

  /** @override */
  run() {
    this.wasRun = true;
    this.runResolver_(this);
  }

  /** Sends a quick error notification. */
  notifyError() {
    this.notify(importer.UpdateType.ERROR);
  }

  /** Sends a quick completion notification. */
  notifyComplete() {
    this.notify(importer.UpdateType.COMPLETE);
  }

  /** Sends a quick cancelled notification. */
  notifyCanceled() {
    this.notify(importer.UpdateType.CANCELED);
  }

  /** Sends a quick progress notification. */
  notifyProgress() {
    this.notify(importer.UpdateType.PROGRESS);
  }

  /** @return {!Promise} A promise that settles once #run is called. */
  whenRun() {
    return this.runPromise_;
  }
}

// Verifies that a queued task gets run.
export function testRunsTask(callback) {
  const task = new TestTask('task0');
  queue.queueTask(task);
  reportPromise(task.whenRun(), callback);
}

// Verifies that multiple queued tasks get run.
export function testRunsTasks(callback) {
  const task0 = new TestTask('task0');
  const task1 = new TestTask('task1');

  // Make the tasks call Task#notifyComplete when they are run.
  task0.whenRun().then(task => {
    task.notifyComplete();
  });
  task1.whenRun().then(task => {
    task.notifyComplete();
  });

  // Enqueue both tasks, and then verify that they were run.
  queue.queueTask(task0);
  queue.queueTask(task1);
  reportPromise(Promise.all([task0.whenRun(), task1.whenRun()]), callback);
}

// Verifies that the active callback triggers when the queue starts doing work
export function testOnActiveCalled(callback) {
  const task = new TestTask('task0');

  // Make a promise that resolves when the active callback is triggered.
  const whenActive = new Promise(resolve => {
    queue.setActiveCallback(() => {
      // Verify that the active callback is called before the task runs.
      assertFalse(task.wasRun);
      resolve();
    });
  });

  // Queue a task, and then check that the active callback was triggered.
  queue.queueTask(task);
  reportPromise(whenActive, callback);
}

// Verifies that the idle callback triggers when the queue is empty.
export function testOnIdleCalled(callback) {
  const task = new TestTask('task0');

  task.whenRun().then(task => {
    task.notifyComplete();
  });

  // Make a promise that resolves when the idle callback is triggered
  // (i.e. after all queued tasks have finished running).
  const whenDone = new Promise(resolve => {
    queue.setIdleCallback(() => {
      // Verify that the idle callback is called after the task runs.
      assertTrue(task.wasRun);
      resolve();
    });
  });

  // Queue a task, then check that the idle callback was triggered.
  queue.queueTask(task);
  reportPromise(whenDone, callback);
}

// Verifies that the update callback is called when a task reports progress.
export function testProgressUpdate(callback) {
  const task = new TestTask('task0');

  // Get the task to report some progress, then success, when it's run.
  task.whenRun()
      .then(task => {
        task.notifyProgress();
        return task;
      })
      .then(task => {
        task.notifyComplete();
        return task;
      });

  // Make a promise that resolves after the task runs.
  const whenDone = new Promise(resolve => {
    queue.setIdleCallback(() => {
      // Verify that progress was recorded.
      assertEquals(1, updates[importer.UpdateType.PROGRESS]);
      resolve();
    });
  });

  queue.queueTask(task);
  reportPromise(whenDone, callback);
}

// Verifies that the update callback is called to report successful task
// completion.
export function testSuccessUpdate(callback) {
  const task = new TestTask('task0');

  // Get the task to report success when it's run.
  task.whenRun().then(task => {
    task.notifyComplete();
  });

  queue.queueTask(task);

  const whenDone = new Promise(resolve => {
    queue.setIdleCallback(() => {
      // Verify that the done callback was called.
      assertEquals(1, updates[importer.UpdateType.COMPLETE]);
      resolve();
    });
  });

  reportPromise(whenDone, callback);
}

// Verifies that the update callback is called to report task errors.
export function testErrorUpdate(callback) {
  const task = new TestTask('task0');

  // Get the task to report an error when it's run.
  task.whenRun().then(task => {
    task.notifyError();
    // Errors are not terminal; still need to signal task completion
    // otherwise the test hangs.
    task.notifyComplete();
  });

  queue.queueTask(task);

  const whenDone = new Promise(resolve => {
    queue.setIdleCallback(() => {
      // Verify that the done callback was called.
      assertEquals(1, updates[importer.UpdateType.ERROR]);
      resolve();
    });
  });

  reportPromise(whenDone, callback);
}

export function testOnTaskCancelled(callback) {
  const task0 = new TestTask('task0');
  const task1 = new TestTask('task1');

  // Make the tasks call Task#notifyComplete when they are run.
  task0.whenRun().then(task => {
    task.notifyCanceled();
  });
  task1.whenRun().then(task => {
    task.notifyComplete();
  });

  // Enqueue both tasks, and then verify that they were run.
  queue.queueTask(task0);
  queue.queueTask(task1);
  reportPromise(Promise.all([task0.whenRun(), task1.whenRun()]), callback);
}

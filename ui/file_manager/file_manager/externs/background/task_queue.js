// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {importer} from '../../common/js/importer_common.js';

const taskQueueInterfaces = {};

/** @interface */
taskQueueInterfaces.TaskQueue = class {
  /**
   * @param {!taskQueueInterfaces.Task} task
   */
  queueTask(task) {}

  /**
   * Sets a callback to be triggered when a task updates.
   * @param {function(string, !taskQueueInterfaces.Task)} callback
   */
  addUpdateCallback(callback) {}

  /**
   * Sets a callback that is triggered each time the queue goes from an idle
   * (i.e. empty with no running tasks) to an active (i.e. having a running
   * task) state.
   * @param {function()} callback
   */
  setActiveCallback(callback) {}

  /**
   * Sets a callback that is triggered each time the queue goes from an active
   * to an idle state.  Also see #setActiveCallback.
   * @param {function()} callback
   */
  setIdleCallback(callback) {}
};

/**
 * Interface for any Task that is to run on the TaskQueue.
 * @interface
 */
taskQueueInterfaces.Task = class {
  constructor() {}

  /**
   * Sets the TaskQueue that will own this task.  The TaskQueue must call this
   * prior to enqueuing a Task.
   * @param {!taskQueueInterfaces.Task.Observer} observer A callback
   *     that will be triggered each time the task has a status update.
   */
  addObserver(observer) {}

  /**
   * Performs the actual work of the Task.  Child classes should implement this.
   */
  run() {}
};

/**
 * Base class for importer tasks.
 * @interface
 */
taskQueueInterfaces.BaseTask = class extends taskQueueInterfaces.Task {
  /**
   * @param {string} taskId
   */
  constructor(taskId) {
    super();
  }

  /** @return {string} The task ID. */
  get taskId() {}

  /**
   * @return {!Promise<!importer.UpdateType>} Resolves when task
   *     is complete, or cancelled, rejects on error.
   */
  get whenFinished() {}

  /**
   * @param {importer.UpdateType} updateType
   * @param {Object=} opt_data
   * @protected
   */
  notify(updateType, opt_data) {}
};

/**
 * A callback that is triggered whenever an update is reported on the observed
 * task.  The first argument is a string specifying the type of the update.
 * Standard values used by all tasks are enumerated in
 * importer.UpdateType, but child classes may add supplementary update
 * types of their own.  The second argument is an Object containing
 * supplementary information pertaining to the update.
 * @typedef {function(!importer.UpdateType, Object=)}
 */
taskQueueInterfaces.Task.Observer;

export {taskQueueInterfaces};

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageRequestTask} from './image_request_task.js';

/**
 * Scheduler for ImageRequestTask objects. Fetches tasks from a queue and
 * processes them synchronously, taking into account priorities. The highest
 * priority is 0.
 * @constructor
 */
export function Scheduler() {
  /**
   * List of tasks waiting to be checked. If these items are available in
   * cache, then they are processed immediately after starting the scheduler.
   * However, if they have to be downloaded, then these tasks are moved
   * to pendingTasks_.
   *
   * @type {Array<ImageRequestTask>}
   * @private
   */
  this.newTasks_ = [];

  /**
   * List of pending tasks for images to be downloaded.
   * @type {Array<ImageRequestTask>}
   * @private
   */
  this.pendingTasks_ = [];

  /**
   * List of tasks being processed.
   * @type {Array<ImageRequestTask>}
   * @private
   */
  this.activeTasks_ = [];

  /**
   * Map of tasks being added to the queue, but not finalized yet. Keyed by
   * the ImageRequestTask id.
   * @type {Object<string, ImageRequestTask>}>
   * @private
   */
  this.tasks_ = {};

  /**
   * If the scheduler has been started.
   * @type {boolean}
   * @private
   */
  this.started_ = false;
}

/**
 * Maximum download tasks to be run in parallel.
 * @type {number}
 * @const
 */
Scheduler.MAXIMUM_IN_PARALLEL = 5;

/**
 * Adds a task to the internal priority queue and executes it when tasks
 * with higher priorities are finished. If the result is cached, then it is
 * processed immediately once the scheduler is started.
 *
 * @param {ImageRequestTask} task A task to be run
 */
Scheduler.prototype.add = function(task) {
  if (!this.started_) {
    this.newTasks_.push(task);
    this.tasks_[task.getId()] = task;
    return;
  }

  // Enqueue the tasks, since already started.
  this.pendingTasks_.push(task);
  this.sortPendingTasks_();

  this.continue_();
};

/**
 * Removes a task from the scheduler (if exists).
 * @param {string} taskId Unique ID of the task.
 */
Scheduler.prototype.remove = function(taskId) {
  const task = this.tasks_[taskId];
  if (!task) {
    return;
  }

  // Remove from the internal queues with pending tasks.
  const newIndex = this.newTasks_.indexOf(task);
  if (newIndex != -1) {
    this.newTasks_.splice(newIndex, 1);
  }
  const pendingIndex = this.pendingTasks_.indexOf(task);
  if (pendingIndex != -1) {
    this.pendingTasks_.splice(pendingIndex, 1);
  }

  // Cancel the task.
  task.cancel();
  delete this.tasks_[taskId];
};

/**
 * Starts handling tasks.
 */
Scheduler.prototype.start = function() {
  this.started_ = true;

  // Process tasks added before scheduler has been started.
  this.pendingTasks_ = this.newTasks_;
  this.sortPendingTasks_();
  this.newTasks_ = [];

  // Start serving enqueued tasks.
  this.continue_();
};

/**
 * Sorts pending tasks by priorities.
 * @private
 */
Scheduler.prototype.sortPendingTasks_ = function() {
  this.pendingTasks_.sort(function(a, b) {
    return a.getPriority() - b.getPriority();
  });
};

/**
 * Processes pending tasks from the queue. There is no guarantee that
 * all of the tasks will be processed at once.
 *
 * @private
 */
Scheduler.prototype.continue_ = function() {
  // Run only up to MAXIMUM_IN_PARALLEL in the same time.
  while (this.pendingTasks_.length &&
         this.activeTasks_.length < Scheduler.MAXIMUM_IN_PARALLEL) {
    const task = this.pendingTasks_.shift();
    this.activeTasks_.push(task);

    // Try to load from cache. If doesn't exist, then download.
    task.loadFromCacheAndProcess(
        this.finish_.bind(this, task), function(currentTask) {
          currentTask.downloadAndProcess(this.finish_.bind(this, currentTask));
        }.bind(this, task));
  }
};

/**
 * Handles finished tasks.
 *
 * @param {ImageRequestTask} task Finished task.
 * @private
 */
Scheduler.prototype.finish_ = function(task) {
  const index = this.activeTasks_.indexOf(task);
  if (index < 0) {
    console.warn('ImageRequestTask not found.');
  }
  this.activeTasks_.splice(index, 1);
  delete this.tasks_[task.getId()];

  // Continue handling the most important tasks (if started).
  if (this.started_) {
    this.continue_();
  }
};

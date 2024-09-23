// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ImageRequestTask} from './image_request_task.js';


// `deviceMemory` might be 0.5 or 0.25, so we normalize to minimum of 2.
const memory = Math.max(2, navigator.deviceMemory);
// For low end devices `hardwareCount` can be low like 4 some other devies are
// low in memory, it will have the value 4 as in 4GB.
const resourceCount = Math.min(navigator.hardwareConcurrency, memory, 10);

/**
 * Maximum download tasks to be run in parallel, for low end devices we expect
 * the result to be 2, higher end devices we expect to be at least 4, but no
 * more than 5.
 */
export const MAXIMUM_IN_PARALLEL = resourceCount / 2;
console.warn(`Image Loader maximum parallel tasks: ${MAXIMUM_IN_PARALLEL}`);

/**
 * Scheduler for ImageRequestTask objects. Fetches tasks from a queue and
 * processes them synchronously, taking into account priorities. The highest
 * priority is 0.
 */
export class Scheduler {
  /**
   * List of tasks waiting to be checked. If these items are available in
   * cache, then they are processed immediately after starting the scheduler.
   * However, if they have to be downloaded, then these tasks are moved to
   * pendingTasks_.
   */
  private newTasks_: ImageRequestTask[] = [];

  /** List of pending tasks for images to be downloaded. */
  private pendingTasks_: ImageRequestTask[] = [];

  /** List of tasks being processed. */
  private activeTasks_: ImageRequestTask[] = [];

  /**
   * Map of tasks being added to the queue, but not finalized yet. Keyed by
   * the ImageRequestTask id.
   */
  private tasks_: Record<string, ImageRequestTask> = {};

  /** If the scheduler has been started. */
  private started_: boolean = false;

  /**
   * Adds a task to the internal priority queue and executes it when tasks
   * with higher priorities are finished. If the result is cached, then it is
   * processed immediately once the scheduler is started.
   */
  add(task: ImageRequestTask) {
    if (!this.started_) {
      this.newTasks_.push(task);
      this.tasks_[task.getId()] = task;
      return;
    }

    // Enqueue the tasks, since already started.
    this.pendingTasks_.push(task);
    this.sortPendingTasks_();

    this.continue_();
  }

  /** Removes a task from the scheduler (if exists). */
  remove(taskId: string) {
    const task = this.tasks_[taskId];
    if (!task) {
      return;
    }

    // Remove from the internal queues with pending tasks.
    const newIndex = this.newTasks_.indexOf(task);
    if (newIndex !== -1) {
      this.newTasks_.splice(newIndex, 1);
    }
    const pendingIndex = this.pendingTasks_.indexOf(task);
    if (pendingIndex !== -1) {
      this.pendingTasks_.splice(pendingIndex, 1);
    }

    // Cancel the task.
    task.cancel();
    delete this.tasks_[taskId];
  }

  /** Starts handling tasks. */
  start() {
    this.started_ = true;

    // Process tasks added before scheduler has been started.
    this.pendingTasks_ = this.newTasks_;
    this.sortPendingTasks_();
    this.newTasks_ = [];

    // Start serving enqueued tasks.
    this.continue_();
  }

  /** Sorts pending tasks by priorities. */
  private sortPendingTasks_() {
    this.pendingTasks_.sort((a, b) => {
      return a.getPriority() - b.getPriority();
    });
  }

  /**
   * Processes pending tasks from the queue. There is no guarantee that
   * all of the tasks will be processed at once.
   */
  private continue_() {
    // Run only up to MAXIMUM_IN_PARALLEL in the same time.
    while (this.pendingTasks_.length > 0 &&
           this.activeTasks_.length < MAXIMUM_IN_PARALLEL) {
      const task = this.pendingTasks_.shift()!;
      this.activeTasks_.push(task);

      // Try to load from cache. If doesn't exist, then download.
      task.loadFromCacheAndProcess(
          () => this.finish_(task),
          () => task.downloadAndProcess(() => this.finish_(task)));
    }
  }

  /** Handles a finished task. */
  private finish_(task: ImageRequestTask) {
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
  }
}

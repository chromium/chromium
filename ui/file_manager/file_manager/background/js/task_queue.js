// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

importer.TaskQueue = importer.TaskQueue || {};

/**
 * A queue of tasks.  Tasks (subclasses of TaskQueue.Task) can be pushed onto
 * the queue.  The queue becomes active whenever it is not empty, and it will
 * begin executing tasks one at a time.  The tasks are executed in a separate
 * asynchronous context.  As each task runs, it can send update notifications
 * which are relayed back to clients via callbacks.  When the queue runs of of
 * tasks, it goes back into an idle state.  Clients can set callbacks which will
 * be triggered whenever the queue transitions between the active and idle
 * states.
 *
 * @implements {importer.TaskQueue}
 */
importer.TaskQueueImpl = class {
  constructor() {
    /** @private {!Array<!importer.TaskQueue.Task>} */
    this.tasks_ = [];

    /** @private {!Array<!function(string, !importer.TaskQueue.Task)>} */
    this.updateCallbacks_ = [];

    /** @private {?function()} */
    this.activeCallback_ = null;

    /** @private {?function()} */
    this.idleCallback_ = null;

    /** @private {boolean} */
    this.active_ = false;
  }

  /**
   * @param {!importer.TaskQueue.Task} task
   */
  queueTask(task) {
    // The Tasks that are pushed onto the queue aren't required to be inherently
    // asynchronous.  This code force task execution to occur asynchronously.
    Promise.resolve().then(() => {
      task.addObserver(this.onTaskUpdate_.bind(this, task));
      this.tasks_.push(task);
      // If more than one task is queued, then the queue is already running.
      if (this.tasks_.length === 1) {
        this.runPending_();
      }
    });
  }

  /**
   * Sets a callback to be triggered when a task updates.
   * @param {function(string, !importer.TaskQueue.Task)} callback
   */
  addUpdateCallback(callback) {
    this.updateCallbacks_.push(callback);
  }

  /**
   * Sets a callback that is triggered each time the queue goes from an idle
   * (i.e. empty with no running tasks) to an active (i.e. having a running
   * task) state.
   * @param {function()} callback
   */
  setActiveCallback(callback) {
    this.activeCallback_ = callback;
  }

  /**
   * Sets a callback that is triggered each time the queue goes from an active
   * to an idle state.  Also see #setActiveCallback.
   * @param {function()} callback
   */
  setIdleCallback(callback) {
    this.idleCallback_ = callback;
  }

  /**
   * Sends out notifications when a task updates.  This is meant to be called by
   * the running tasks owned by this queue.
   * @param {!importer.TaskQueue.Task} task
   * @param {!importer.TaskQueue.UpdateType} updateType
   * @private
   */
  onTaskUpdate_(task, updateType) {
    // Send a task update to clients.
    this.updateCallbacks_.forEach(callback => {
      callback.call(null, updateType, task);
    });

    // If the task update is a terminal one, move on to the next task.
    const UpdateType = importer.TaskQueue.UpdateType;
    if (updateType === UpdateType.COMPLETE ||
        updateType === UpdateType.CANCELED) {
      // Assumption: the currently running task is at the head of the queue.
      console.assert(
          this.tasks_[0] === task,
          'Only tasks that are at the head of the queue should be active');
      // Remove the completed task from the queue.
      this.tasks_.shift();
      // Run the next thing in the queue.
      this.runPending_();
    }
  }

  /**
   * Wakes the task queue up and runs the next pending task, or makes the queue
   * go back to sleep if no tasks are pending.
   * @private
   */
  runPending_() {
    if (this.tasks_.length === 0) {
      // All done - go back to idle.
      this.active_ = false;
      if (this.idleCallback_) {
        this.idleCallback_();
      }
      return;
    }

    if (!this.active_) {
      // If the queue is currently idle, transition to active state.
      this.active_ = true;
      if (this.activeCallback_) {
        this.activeCallback_();
      }
    }

    const nextTask = this.tasks_[0];
    nextTask.run();
  }
};


/**
 * Base class for importer tasks.
 * @implements {importer.TaskQueue.Task}
 */
importer.TaskQueue.BaseTaskImpl = class {
  /**
   * @param {string} taskId
   */
  constructor(taskId) {
    /** @protected {string} */
    this.taskId_ = taskId;

    /** @private {!Array<!importer.TaskQueue.Task.Observer>} */
    this.observers_ = [];

    /** @private {!importer.Resolver<!importer.TaskQueue.UpdateType>} */
    this.finishedResolver_ = new importer.Resolver();
  }
  /** @return {string} The task ID. */
  get taskId() {
    return this.taskId_;
  }

  /**
   * @return {!Promise<!importer.TaskQueue.UpdateType>} Resolves when task
   *     is complete, or cancelled, rejects on error.
   */
  get whenFinished() {
    return this.finishedResolver_.promise;
  }

  /** @override */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /** @override */
  run() {}

  /**
   * @param {importer.TaskQueue.UpdateType} updateType
   * @param {Object=} opt_data
   * @protected
   */
  notify(updateType, opt_data) {
    switch (updateType) {
      case importer.TaskQueue.UpdateType.CANCELED:
      case importer.TaskQueue.UpdateType.COMPLETE:
        this.finishedResolver_.resolve(updateType);
    }

    this.observers_.forEach(callback => {
      callback.call(null, updateType, opt_data);
    });
  }
};

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be started in order they were added. Tasks are run
 * concurrently. At most, |limit| jobs will be run at the same time.
 */
export class ConcurrentQueue {
  /**
   * @param {number} limit The number of tasks to run at the same time.
   */
  constructor(limit) {
    console.assert(limit > 0, '|limit| must be larger than 0');
    this.limit_ = limit;
    // @ts-ignore: error TS7008: Member 'added_' implicitly has an 'any[]' type.
    this.added_ = [];
    // @ts-ignore: error TS7008: Member 'running_' implicitly has an 'any[]'
    // type.
    this.running_ = [];
    this.cancelled_ = false;
  }

  /**
   * @return {boolean} True when a task is running, otherwise false.
   */
  isRunning() {
    return this.running_.length !== 0;
  }

  /**
   * @return {number} Number of waiting tasks.
   */
  getWaitingTasksCount() {
    return this.added_.length;
  }

  /**
   * @return {number} Number of running tasks.
   */
  getRunningTasksCount() {
    return this.running_.length;
  }

  /**
   * Enqueues a task for running as soon as possible. If there is already the
   * maximum number of tasks running, the run of this task is delayed until less
   * than the limit given at the construction time of tasks are running.
   * @param {function(function():void):void} task The task to be enqueued for
   *     execution.
   */
  run(task) {
    if (this.cancelled_) {
      console.warn('Queue is cancelled. Cannot add a new task.');
    } else {
      this.added_.push(task);
      this.scheduleNext_();
    }
  }

  /**
   * Cancels the queue. It removes all the not-run (yet) tasks. Note that this
   * does NOT stop tasks currently running.
   */
  cancel() {
    this.cancelled_ = true;
    this.added_ = [];
  }

  /**
   * @return {boolean} True when the queue have been requested to cancel or is
   *      already cancelled. Otherwise false.
   */
  isCancelled() {
    return this.cancelled_;
  }

  /**
   * Attempts to run another tasks. If there is less than the maximum number
   * of task running, it immediately executes the task at the front of
   * the queue.
   */
  maybeExecute_() {
    if (this.added_.length > 0) {
      if (this.running_.length < this.limit_) {
        this.execute_(this.added_.shift());
      }
    }
  }

  /**
   * Executes the given task. The task is placed in the list of running tasks
   * and immediately executed.
   * @param {function(function():void):void} task The task to be immediately
   *     executed.
   */
  execute_(task) {
    this.running_.push(task);
    try {
      task(this.onTaskFinished_.bind(this, task));
      // If the task executes successfully, it calls the callback, where we
      // schedule a next run.
    } catch (e) {
      console.warn('Failed to execute a task', e);
      // If the task fails we call the callback explicitly.
      this.onTaskFinished_(task);
    }
  }

  /**
   * Handles a task being finished.
   */
  // @ts-ignore: error TS7006: Parameter 'task' implicitly has an 'any' type.
  onTaskFinished_(task) {
    this.removeTask_(task);
    this.scheduleNext_();
  }

  /**
   * Attempts to remove the task that was running.
   */
  // @ts-ignore: error TS7006: Parameter 'task' implicitly has an 'any' type.
  removeTask_(task) {
    const index = this.running_.indexOf(task);
    if (index >= 0) {
      this.running_.splice(index, 1);
    } else {
      console.warn('Failed to find a finished task among running');
    }
  }

  /**
   * Schedules the next attempt at execution of the task at the front of
   * the queue.
   */
  scheduleNext_() {
    // TODO(1350885): Use setTimeout(()=>{this.maybeExecute();});
    this.maybeExecute_();
  }

  /**
   * Returns string representation of current ConcurrentQueue
   * instance.
   * @return {string} String representation of the instance.
   */
  toString() {
    return 'ConcurrentQueue\n' +
        '- WaitingTasksCount: ' + this.getWaitingTasksCount() + '\n' +
        '- RunningTasksCount: ' + this.getRunningTasksCount() + '\n' +
        '- isCancelled: ' + this.isCancelled();
  }
}

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be executed sequentially in order they were added.
 */
export class AsyncQueue extends ConcurrentQueue {
  constructor() {
    super(1);
  }

  /**
   * Starts a task gated by this concurrent queue.
   * Typical usage:
   *
   *   const unlock = await queue.lock();
   *   try {
   *     // Operations of the task.
   *     ...
   *   } finally {
   *     unlock();
   *   }
   *
   * @return {!Promise<function()>} Completion callback to run when finished.
   */
  async lock() {
    return new Promise(resolve => this.run(unlock => resolve(unlock)));
  }
}

/**
 * A task which is executed by Group.
 */
export class GroupTask {
  /**
   * @param {!function(function():void):void} closure Closure with a completion
callback
   *     to be executed.
   * @param {!Array<string>} dependencies Array of dependencies.
   * @param {!string} name Task identifier. Specify to use in dependencies.
   */
  constructor(closure, dependencies, name) {
    this.closure = closure;
    this.dependencies = dependencies;
    this.name = name;
  }

  /**
   * Returns string representation of GroupTask instance.
   * @return {string} String representation of the instance.
   */
  toString() {
    return 'GroupTask\n' +
        '- name: ' + this.name + '\n' +
        '- dependencies: ' + this.dependencies.join();
  }
}

/**
 * Creates a class for executing several asynchronous closures in a group in
 * a dependency order.
 */
export class Group {
  constructor() {
    this.addedTasks_ = {};
    this.pendingTasks_ = {};
    this.finishedTasks_ = {};
    // @ts-ignore: error TS7008: Member 'completionCallbacks_' implicitly has an
    // 'any[]' type.
    this.completionCallbacks_ = [];
  }

  /**
   * @return {!Record<string, GroupTask>} Pending tasks
   */
  get pendingTasks() {
    return this.pendingTasks_;
  }

  /**
   * Enqueues a closure to be executed after dependencies are completed.
   *
   * @param {function(function():void):void} closure Closure with a completion
   *     callback to be executed.
   * @param {Array<string>=} opt_dependencies Array of dependencies. If no
   *     dependencies, then the the closure will be executed immediately.
   * @param {string=} opt_name Task identifier. Specify to use in dependencies.
   */
  add(closure, opt_dependencies, opt_name) {
    const length = Object.keys(this.addedTasks_).length;
    const name = opt_name || ('(unnamed#' + (length + 1) + ')');

    const task = new GroupTask(closure, opt_dependencies || [], name);

    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.addedTasks_[name] = task;
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.pendingTasks_[name] = task;
  }

  /**
   * Runs the enqueued closured in order of dependencies.
   *
   * @param {function()=} opt_onCompletion Completion callback.
   */
  run(opt_onCompletion) {
    if (opt_onCompletion) {
      this.completionCallbacks_.push(opt_onCompletion);
    }
    this.continue_();
  }

  /**
   * Runs enqueued pending tasks whose dependencies are completed.
   * @private
   */
  continue_() {
    // If all of the added tasks have finished, then call completion callbacks.
    if (Object.keys(this.addedTasks_).length ==
        Object.keys(this.finishedTasks_).length) {
      for (let index = 0; index < this.completionCallbacks_.length; index++) {
        const callback = this.completionCallbacks_[index];
        callback();
      }
      this.completionCallbacks_ = [];
      return;
    }

    for (const name in this.pendingTasks_) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      const task = this.pendingTasks_[name];
      let dependencyMissing = false;
      for (let index = 0; index < task.dependencies.length; index++) {
        const dependency = task.dependencies[index];
        // Check if the dependency has finished.
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'any' can't be used to index type '{}'.
        if (!this.finishedTasks_[dependency]) {
          dependencyMissing = true;
        }
      }
      // All dependences finished, therefore start the task.
      if (!dependencyMissing) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'any' can't be used to index type '{}'.
        delete this.pendingTasks_[task.name];
        task.closure(this.finish_.bind(this, task));
      }
    }
  }

  /**
   * Finishes the passed task and continues executing enqueued closures.
   *
   * @param {Object} task Task object.
   * @private
   */
  finish_(task) {
    // @ts-ignore: error TS2339: Property 'name' does not exist on type
    // 'Object'.
    this.finishedTasks_[task.name] = task;
    this.continue_();
  }
}

/**
 * Aggregates consecutive calls and executes the closure only once instead of
 * several times. The first call is always called immediately, and the next
 * consecutive ones are aggregated and the closure is called only once once
 * |delay| amount of time passes after the last call to run().
 */
export class Aggregator {
  /**
   * @param {function():void} closure Closure to be aggregated.
   * @param {number=} opt_delay Minimum aggregation time in milliseconds.
   *     Default is 50 milliseconds.
   */
  constructor(closure, opt_delay) {
    /**
     * @type {number}
     * @private
     */
    this.delay_ = opt_delay || 50;

    /**
     * @type {function():void}
     * @private
     */
    this.closure_ = closure;

    /**
     * @type {number?}
     * @private
     */
    this.scheduledRunsTimer_ = null;

    /**
     * @type {number}
     * @private
     */
    this.lastRunTime_ = 0;
  }

  /**
   * Runs a closure. Skips consecutive calls. The first call is called
   * immediately.
   */
  run() {
    // If recently called, then schedule the consecutive call with a delay.
    if (Date.now() - this.lastRunTime_ < this.delay_) {
      this.cancelScheduledRuns_();
      this.scheduledRunsTimer_ =
          setTimeout(this.runImmediately_.bind(this), this.delay_ + 1);
      this.lastRunTime_ = Date.now();
      return;
    }

    // Otherwise, run immediately.
    this.runImmediately_();
  }

  /**
   * Calls the schedule immediately and cancels any scheduled calls.
   * @private
   */
  runImmediately_() {
    this.cancelScheduledRuns_();
    this.closure_();
    this.lastRunTime_ = Date.now();
  }

  /**
   * Cancels all scheduled runs (if any).
   * @private
   */
  cancelScheduledRuns_() {
    if (this.scheduledRunsTimer_) {
      clearTimeout(this.scheduledRunsTimer_);
      this.scheduledRunsTimer_ = null;
    }
  }
}

/**
 * Samples calls so that they are not called too frequently.
 * The first call is always called immediately, and the following calls may
 * be skipped or delayed to keep each interval no less than |minInterval_|.
 */
export class RateLimiter {
  /**
   * @param {function():void} closure Closure to be called.
   * @param {number=} opt_minInterval Minimum interval between each call in
   *     milliseconds. Default is 200 milliseconds.
   */
  constructor(closure, opt_minInterval) {
    /**
     * @type {function():void}
     * @private
     */
    this.closure_ = closure;

    /**
     * @type {number}
     * @private
     */
    this.minInterval_ = opt_minInterval || 200;

    /**
     * @type {number}
     * @private
     */
    this.scheduledRunsTimer_ = 0;

    /**
     * This variable remembers the last time the closure is called.
     * @type {number}
     * @private
     */
    this.lastRunTime_ = 0;
  }

  /**
   * Requests to run the closure.
   * Skips or delays calls so that the intervals between calls are no less than
   * |minInterval_| milliseconds.
   */
  run() {
    const now = Date.now();
    // If |minInterval| has not passed since the closure is run, skips or delays
    // this run.
    if (now - this.lastRunTime_ < this.minInterval_) {
      // Delays this run only when there is no scheduled run.
      // Otherwise, simply skip this run.
      if (!this.scheduledRunsTimer_) {
        this.scheduledRunsTimer_ = setTimeout(
            this.runImmediately.bind(this),
            this.lastRunTime_ + this.minInterval_ - now);
      }
      return;
    }

    // Otherwise, run immediately
    this.runImmediately();
  }

  /**
   * Calls the scheduled run immediately and cancels any scheduled calls.
   */
  runImmediately() {
    this.cancelScheduledRuns_();
    this.lastRunTime_ = Date.now();
    this.closure_();
  }

  /**
   * Cancels all scheduled runs (if any).
   * @private
   */
  cancelScheduledRuns_() {
    if (this.scheduledRunsTimer_) {
      clearTimeout(this.scheduledRunsTimer_);
      this.scheduledRunsTimer_ = 0;
    }
  }
}

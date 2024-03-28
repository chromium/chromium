// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type Task = (callback: VoidCallback) => void;

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be started in order they were added. Tasks are run
 * concurrently. At most, |limit| jobs will be run at the same time.
 */
export class ConcurrentQueue {
  private readonly added_: Task[] = [];
  private readonly running_: Task[] = [];
  private cancelled_ = false;

  /** @param limit_ The number of tasks to run at the same time. */
  constructor(private limit_: number) {
    console.assert(this.limit_ > 0, 'limit_ must be larger than 0');
  }

  /** @return whether a task is running. */
  isRunning(): boolean {
    return this.running_.length !== 0;
  }

  /** @return the number of waiting tasks. */
  getWaitingTasksCount(): number {
    return this.added_.length;
  }

  /** @return the number of running tasks. */
  getRunningTasksCount(): number {
    return this.running_.length;
  }

  /**
   * Enqueues a task for running as soon as possible. If there is already the
   * maximum number of tasks running, the run of this task is delayed until less
   * than the limit given at the construction time of tasks are running.
   */
  run(task: Task) {
    if (this.cancelled_) {
      console.warn('Cannot add a new task: Queue is cancelled');
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
    this.added_.length = 0;
  }

  /** @return whether the queue is cancelling or is already cancelled. */
  isCanceled(): boolean {
    return this.cancelled_;
  }

  /**
   * Attempts to run another tasks. If there is less than the maximum number
   * of task running, it immediately executes the task at the front of
   * the queue.
   */
  private maybeExecute_() {
    if (this.added_.length > 0) {
      if (this.running_.length < this.limit_) {
        this.execute_(this.added_.shift()!);
      }
    }
  }

  /**
   * Executes the given `task`. The task is placed in the list of running tasks
   * and immediately executed.
   */
  private execute_(task: Task) {
    this.running_.push(task);
    try {
      task(this.onTaskFinished_.bind(this, task));
      // If the task executes successfully, it calls the callback, where we
      // schedule a next run.
    } catch (e) {
      console.warn('Cannot execute a task', e);
      // If the task fails we call the callback explicitly.
      this.onTaskFinished_(task);
    }
  }

  /** Handles a task being finished. */
  private onTaskFinished_(task: Task) {
    this.removeTask_(task);
    this.scheduleNext_();
  }

  /** Attempts to remove the task that was running. */
  private removeTask_(task: Task) {
    const index = this.running_.indexOf(task);
    if (index >= 0) {
      this.running_.splice(index, 1);
    } else {
      console.warn('Cannot find a finished task among the running ones');
    }
  }

  /**
   * Schedules the next attempt at execution of the task at the front of
   * the queue.
   */
  private scheduleNext_() {
    // TODO(1350885): Use setTimeout(()=>{this.maybeExecute();});
    this.maybeExecute_();
  }

  /** @return a string representation of the instance. */
  toString(): string {
    return 'ConcurrentQueue\n' +
        '- WaitingTasksCount: ' + this.getWaitingTasksCount() + '\n' +
        '- RunningTasksCount: ' + this.getRunningTasksCount() + '\n' +
        '- isCanceled: ' + this.isCanceled();
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
   * @return Completion callback to run when finished.
   */
  async lock(): Promise<VoidCallback> {
    return new Promise(resolve => this.run(unlock => resolve(unlock)));
  }
}

/** A task which is executed by Group. */
export class GroupTask {
  /**
   * @param closure Closure with a completion callback to be executed.
   * @param dependencies Array of dependencies.
   * @param name Task identifier. Specify to use in dependencies.
   */
  constructor(
      public readonly closure: Task, public readonly dependencies: string[],
      public readonly name: string) {}

  /** @return a string representation of the instance. */
  toString(): string {
    return 'GroupTask\n' +
        '- name: ' + this.name + '\n' +
        '- dependencies: ' + this.dependencies.join();
  }
}

export type GroupTasks = Record<string, GroupTask>;

/**
 * Creates a class for executing several asynchronous closures in a group in a
 * dependency order.
 */
export class Group {
  private readonly addedTasks_: GroupTasks = {};
  private readonly pendingTasks_: GroupTasks = {};
  private readonly finishedTasks_: GroupTasks = {};
  private readonly completionCallbacks_: VoidCallback[] = [];

  /** @return the pending tasks. */
  get pendingTasks(): GroupTasks {
    return this.pendingTasks_;
  }

  /**
   * Enqueues a closure to be executed after dependencies are completed.
   *
   * @param closure Closure with a completion callback to be executed.
   * @param dependencies Array of dependencies. If no dependencies, then the
   *     the closure will be executed immediately.
   * @param maybeName Task identifier. Specify to use in dependencies.
   */
  add(closure: Task, dependencies: string[] = [], maybeName?: string) {
    const name =
        maybeName || (`(unnamed#${Object.keys(this.addedTasks_).length + 1})`);
    const task = new GroupTask(closure, dependencies, name);
    this.addedTasks_[name] = task;
    this.pendingTasks_[name] = task;
  }

  /**
   * Runs the enqueued closure in order of dependencies.
   * @param onCompletion Completion callback.
   */
  run(onCompletion?: VoidCallback) {
    if (onCompletion) {
      this.completionCallbacks_.push(onCompletion);
    }
    this.continue_();
  }

  /** Runs enqueued pending tasks whose dependencies are completed. */
  private continue_() {
    // If all of the added tasks have finished, then call completion callbacks.
    if (Object.keys(this.addedTasks_).length ===
        Object.keys(this.finishedTasks_).length) {
      for (const callback of this.completionCallbacks_) {
        callback();
      }
      this.completionCallbacks_.length = 0;
      return;
    }

    for (const name in this.pendingTasks_) {
      const task = this.pendingTasks_[name]!;
      let dependencyMissing = false;
      for (const dependency of task.dependencies) {
        // Check if the dependency has finished.
        if (!this.finishedTasks_[dependency]) {
          dependencyMissing = true;
        }
      }
      // All dependences finished, therefore start the task.
      if (!dependencyMissing) {
        delete this.pendingTasks_[task.name];
        task.closure(this.finish_.bind(this, task));
      }
    }
  }

  /** Finishes the passed task and continues executing enqueued closures. */
  private finish_(task: GroupTask) {
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
  private scheduledRunsTimer_: number|null = null;
  private lastRunTime_ = 0;

  /**
   * @param closure_ Closure to be aggregated.
   * @param delay_ Minimum aggregation time in milliseconds.
   */
  constructor(private closure_: VoidCallback, private delay_: number = 50) {}

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

  /** Calls the schedule immediately and cancels any scheduled calls. */
  private runImmediately_() {
    this.cancelScheduledRuns_();
    this.closure_();
    this.lastRunTime_ = Date.now();
  }

  /** Cancels all scheduled runs (if any). */
  private cancelScheduledRuns_() {
    if (this.scheduledRunsTimer_) {
      clearTimeout(this.scheduledRunsTimer_);
      this.scheduledRunsTimer_ = null;
    }
  }
}

/**
 * Samples calls so that they are not called too frequently. The first call is
 * always called immediately, and the following calls may be skipped or delayed
 * to keep each interval no less than `minInterval_`.
 */
export class RateLimiter {
  private scheduledRunsTimer_ = 0;

  /** Last time the closure is called. */
  private lastRunTime_ = 0;

  /**
   * @param closure_ Closure to be called.
   * @param minInterval_ Minimum interval between each call in milliseconds.
   */
  constructor(
      private closure_: VoidCallback, private minInterval_: number = 200) {}

  /**
   * Requests to run the closure. Skips or delays calls so that the intervals
   * between calls are no less than `minInterval_` milliseconds.
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

  /** Calls the scheduled run immediately and cancels any scheduled calls. */
  runImmediately() {
    this.cancelScheduledRuns_();
    this.lastRunTime_ = Date.now();
    this.closure_();
  }

  /** Cancels all scheduled runs (if any). */
  private cancelScheduledRuns_() {
    if (this.scheduledRunsTimer_) {
      clearTimeout(this.scheduledRunsTimer_);
      this.scheduledRunsTimer_ = 0;
    }
  }
}

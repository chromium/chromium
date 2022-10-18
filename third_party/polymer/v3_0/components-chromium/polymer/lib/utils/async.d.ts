// tslint:disable:variable-name Describing an API that's defined elsewhere.

/**
 * Async interface wrapper around `setTimeout`.
 */
declare namespace timeOut {


  /**
   * Returns a sub-module with the async interface providing the provided
   * delay.
   *
   * @returns An async timeout interface
   */
  function after(delay?: number): AsyncInterface;


  /**
   * Enqueues a function called in the next task.
   *
   * @returns Handle used for canceling task
   */
  function run(fn: Function, delay?: number): number;


  /**
   * Cancels a previously enqueued `timeOut` callback.
   */
  function cancel(handle: number): void;
}

export {timeOut};

/**
 * Async interface wrapper around `requestAnimationFrame`.
 */
declare namespace animationFrame {


  /**
   * Enqueues a function called at `requestAnimationFrame` timing.
   *
   * @returns Handle used for canceling task
   */
  function run(fn: (p0: number) => void): number;


  /**
   * Cancels a previously enqueued `animationFrame` callback.
   */
  function cancel(handle: number): void;
}

export {animationFrame};

/**
 * Async interface wrapper around `requestIdleCallback`.  Falls back to
 * `setTimeout` on browsers that do not support `requestIdleCallback`.
 */
declare namespace idlePeriod {


  /**
   * Enqueues a function called at `requestIdleCallback` timing.
   *
   * @returns Handle used for canceling task
   */
  function run(fn: (p0: IdleDeadline) => void): number;


  /**
   * Cancels a previously enqueued `idlePeriod` callback.
   */
  function cancel(handle: number): void;
}

export {idlePeriod};

/**
 * Async interface for enqueuing callbacks that run at microtask timing.
 *
 * Note that microtask timing is achieved via a single `MutationObserver`,
 * and thus callbacks enqueued with this API will all run in a single
 * batch, and not interleaved with other microtasks such as promises.
 * Promises are avoided as an implementation choice for the time being
 * due to Safari bugs that cause Promises to lack microtask guarantees.
 */
declare namespace microTask {


  /**
   * Enqueues a function called at microtask timing.
   *
   * @returns Handle used for canceling task
   */
  function run(callback?: Function): number;


  /**
   * Cancels a previously enqueued `microTask` callback.
   */
  function cancel(handle: number): void;
}

export {microTask};

import {AsyncInterface} from '../../interfaces';

import {IdleDeadline} from '../../interfaces';

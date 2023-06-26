/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Easing functions to use for web animations.
 *
 * **NOTE:** `EASING.EMPHASIZED` is approximated with unknown accuracy.
 *
 * TODO(b/241113345): replace with tokens
 */
export declare const EASING: {
    readonly STANDARD: "cubic-bezier(0.2, 0, 0, 1)";
    readonly STANDARD_ACCELERATE: "cubic-bezier(.3,0,1,1)";
    readonly STANDARD_DECELERATE: "cubic-bezier(0,0,0,1)";
    readonly EMPHASIZED: "cubic-bezier(.3,0,0,1)";
    readonly EMPHASIZED_ACCELERATE: "cubic-bezier(.3,0,.8,.15)";
    readonly EMPHASIZED_DECELERATE: "cubic-bezier(.05,.7,.1,1)";
};
/**
 * A signal that is used for abortable tasks.
 */
export interface AnimationSignal {
    /**
     * Starts the abortable task. Any previous tasks started with this instance
     * will be aborted.
     *
     * @return An `AbortSignal` for the current task.
     */
    start(): AbortSignal;
    /**
     * Complete the current task.
     */
    finish(): void;
}
/**
 * Creates an `AnimationSignal` that can be used to cancel a previous task.
 *
 * @example
 * class MyClass {
 *   private labelAnimationSignal = createAnimationSignal();
 *
 *   private async animateLabel() {
 *     // Start of the task. Previous tasks will be canceled.
 *     const signal = this.labelAnimationSignal.start();
 *
 *     // Do async work...
 *     if (signal.aborted) {
 *       // Use AbortSignal to check if a request was made to abort after some
 *       // asynchronous work.
 *       return;
 *     }
 *
 *     const animation = this.animate(...);
 *     // Add event listeners to be notified when the task should be canceled.
 *     signal.addEventListener('abort', () => {
 *       animation.cancel();
 *     });
 *
 *     animation.addEventListener('finish', () => {
 *       // Tell the signal that the current task is finished.
 *       this.labelAnimationSignal.finish();
 *     });
 *   }
 * }
 *
 * @return An `AnimationSignal`.
 */
export declare function createAnimationSignal(): AnimationSignal;
/**
 * Returns a function which can be used to throttle function calls
 * mapped to a key via a given function that should produce a promise that
 * determines the throttle amount (defaults to requestAnimationFrame).
 */
export declare function createThrottle(): (key: string, cb: (...args: unknown[]) => unknown, timeout?: () => Promise<void>) => Promise<void>;
/**
 * Parses an number in milliseconds from a css time value
 */
export declare function msFromTimeCSSValue(value: string): number;

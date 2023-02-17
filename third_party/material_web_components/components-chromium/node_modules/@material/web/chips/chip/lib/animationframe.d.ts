/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * AnimationFrame provides a user-friendly abstraction around requesting
 * and canceling animation frames.
 */
export declare class AnimationFrame {
    private readonly rafIDs;
    /**
     * Requests an animation frame. Cancels any existing frame with the same key.
     * @param {string} key The key for this callback.
     * @param {FrameRequestCallback} callback The callback to be executed.
     */
    request(key: string, callback: FrameRequestCallback): void;
    /**
     * Cancels a queued callback with the given key.
     * @param {string} key The key for this callback.
     */
    cancel(key: string): void;
    /**
     * Cancels all queued callback.
     */
    cancelAll(): void;
    /**
     * Returns the queue of unexecuted callback keys.
     */
    getQueue(): string[];
}

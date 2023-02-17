/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { Foundation } from './foundation.js';
import { ObserverRecord } from './observer.js';
/**
 * Legacy observer foundation class for components.
 */
export declare class ObserverFoundation<Adapter extends object> extends Foundation<Adapter> {
    /**
     * Observe a target's properties for changes using the provided map of
     * property names and observer functions.
     *
     * @template T The target type.
     * @param target - The target to observe.
     * @param observers - An object whose keys are target properties and values
     *     are observer functions that are called when the associated property
     *     changes.
     * @return A cleanup function that can be called to unobserve the
     *     target.
     */
    protected observe<T extends object>(target: T, observers: ObserverRecord<T, this>): () => void;
}

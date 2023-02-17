/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * A function used to observe property changes on a target.
 *
 * @template T The observed target type.
 * @template K The observed property.
 * @template This The `this` context of the observer function.
 * @param current - The current value of the property.
 * @param previous - The previous value of the property.
 */
export declare type Observer<T extends object, K extends keyof T = keyof T, This = unknown> = (this: This, current: T[K], previous: T[K]) => void;
/**
 * An object map whose keys are properties of a target to observe and values
 * are `Observer` functions for each property.
 *
 * @template T The observed target type.
 * @template This The `this` context of observer functions.
 */
export declare type ObserverRecord<T extends object, This = unknown> = {
    [K in keyof T]?: Observer<T, K, This>;
};
/**
 * Observe a target's property for changes. When a property changes, the
 * provided `Observer` function will be invoked with the properties current and
 * previous values.
 *
 * The returned cleanup function will stop listening to changes for the
 * provided `Observer`.
 *
 * @template T The observed target type.
 * @template K The observed property.
 * @param target - The target to observe.
 * @param property - The property of the target to observe.
 * @param observer - An observer function to invoke each time the property
 *     changes.
 * @return A cleanup function that will stop observing changes for the provided
 *     `Observer`.
 */
export declare function observeProperty<T extends object, K extends keyof T>(target: T, property: K, observer: Observer<T, K>): () => void;
/**
 * Enables or disables all observers for a provided target. Changes to observed
 * properties will not call any observers when disabled.
 *
 * @template T The observed target type.
 * @param target - The target to enable or disable observers for.
 * @param enabled - True to enable or false to disable observers.
 */
export declare function setObserversEnabled<T extends object>(target: T, enabled: boolean): void;

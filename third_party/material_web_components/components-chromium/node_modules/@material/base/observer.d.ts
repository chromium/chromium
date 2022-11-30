/**
 * @license
 * Copyright 2021 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
import { Constructor } from './types';
/**
 * A class that can observe targets and perform cleanup logic. Classes may
 * implement this using the `mdcObserver()` mixin.
 */
export interface MDCObserver {
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
    observe<T extends object>(target: T, observers: ObserverRecord<T, this>): () => void;
    /**
     * Enables or disables all observers for the provided target. Disabling
     * observers will prevent them from being called until they are re-enabled.
     *
     * @param target - The target to enable or disable observers for.
     * @param enabled - Whether or not observers should be called.
     */
    setObserversEnabled(target: object, enabled: boolean): void;
    /**
     * Clean up all observers and stop listening for property changes.
     */
    unobserve(): void;
}
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
 * Mixin to add `MDCObserver` functionality.
 *
 * @deprecated Prefer MDCObserverFoundation for stricter closure compliance.
 * @return A class with `MDCObserver` functionality.
 */
export declare function mdcObserver(): Constructor<MDCObserver>;
/**
 * Mixin to add `MDCObserver` functionality to a base class.
 *
 * @deprecated Prefer MDCObserverFoundation for stricter closure compliance.
 * @template T Base class instance type. Specify this generic if the base class
 *     itself has generics that cannot be inferred.
 * @template C Base class constructor type.
 * @param baseClass - Base class.
 * @return A class that extends the optional base class with `MDCObserver`
 *     functionality.
 */
export declare function mdcObserver<T, C extends Constructor<T>>(baseClass: C): Constructor<MDCObserver> & Constructor<T> & C;
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
 * Retrieves the descriptor for a property from the provided target. This
 * function will walk up the target's prototype chain to search for the
 * descriptor.
 *
 * @template T The target type.
 * @template K The property type.
 * @param target - The target to retrieve a descriptor from.
 * @param property - The name of the property to retrieve a descriptor for.
 * @return the descriptor, or undefined if it does not exist. Keep in mind that
 *     plain properties may not have a descriptor defined.
 */
export declare function getDescriptor<T extends object, K extends keyof T>(target: T, property: K): TypedPropertyDescriptor<T[K]> | undefined;
/**
 * Enables or disables all observers for a provided target. Changes to observed
 * properties will not call any observers when disabled.
 *
 * @template T The observed target type.
 * @param target - The target to enable or disable observers for.
 * @param enabled - True to enable or false to disable observers.
 */
export declare function setObserversEnabled<T extends object>(target: T, enabled: boolean): void;

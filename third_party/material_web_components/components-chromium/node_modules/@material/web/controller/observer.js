/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
const isTargetObservers = Symbol();
const isEnabled = Symbol();
const getObservers = Symbol();
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
export function observeProperty(target, property, observer) {
    const observerPrototype = installObserver(target);
    const observers = observerPrototype[getObservers](property);
    observers.push(observer);
    return () => {
        observers.splice(observers.indexOf(observer), 1);
    };
}
/**
 * Installs a `TargetObservers` for the provided target (if not already
 * installed).
 *
 * A target's `TargetObservers` is installed as a Proxy on the target's
 * prototype.
 *
 * @template T The observed target type.
 * @param obj - The target to observe.
 * @return The installed `TargetObservers` for the provided target.
 */
function installObserver(obj) {
    const prototype = Object.getPrototypeOf(obj);
    if (prototype[isTargetObservers]) {
        return prototype;
    }
    // Proxy prototypes will not trap plain properties (not a getter/setter) that
    // are already defined. They only work on new plain properties.
    // We can work around this by deleting the properties, installing the Proxy,
    // then re-setting the properties.
    const existingKeyValues = new Map();
    const keys = Object.getOwnPropertyNames(obj);
    for (const key of keys) {
        const descriptor = getDescriptor(obj, key);
        if (descriptor && descriptor.writable) {
            existingKeyValues.set(key, descriptor.value);
            delete obj[key];
        }
    }
    const proxy = new Proxy(Object.create(prototype), {
        get(target, key, receiver) {
            return Reflect.get(target, key, receiver);
        },
        set(target, key, newValue, receiver) {
            const isTargetObserversKey = key === isTargetObservers ||
                key === isEnabled || key === getObservers;
            const previous = Reflect.get(target, key, receiver);
            // If a key has an existing setter, invoke it with the receiver to
            // preserve the correct `this` context.
            // Otherwise, the key is either a new or existing plain property and
            // should be set on the target. Setting a plain property on the
            // receiver will cause the proxy to no longer be able to observe it.
            const descriptor = getDescriptor(target, key);
            if (descriptor?.set) {
                Reflect.set(target, key, newValue, receiver);
            }
            else {
                Reflect.set(target, key, newValue);
            }
            if (!isTargetObserversKey && proxy[isEnabled] &&
                newValue !== previous) {
                for (const observer of proxy[getObservers](key)) {
                    observer(newValue, previous);
                }
            }
            return true;
        }
    });
    proxy[isTargetObservers] = true;
    proxy[isEnabled] = true;
    const observersMap = new Map();
    proxy[getObservers] = (key) => {
        const observers = observersMap.get(key) || [];
        if (!observersMap.has(key)) {
            observersMap.set(key, observers);
        }
        return observers;
    };
    Object.setPrototypeOf(obj, proxy);
    // Re-set plain pre-existing properties so that the Proxy can trap them
    for (const [key, value] of existingKeyValues.entries()) {
        obj[key] = value;
    }
    return proxy;
}
/**
 * Enables or disables all observers for a provided target. Changes to observed
 * properties will not call any observers when disabled.
 *
 * @template T The observed target type.
 * @param target - The target to enable or disable observers for.
 * @param enabled - True to enable or false to disable observers.
 */
export function setObserversEnabled(target, enabled) {
    const prototype = Object.getPrototypeOf(target);
    if (prototype[isTargetObservers]) {
        prototype[isEnabled] = enabled;
    }
}
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
function getDescriptor(target, property) {
    let descriptorTarget = target;
    let descriptor;
    while (descriptorTarget) {
        descriptor = Object.getOwnPropertyDescriptor(descriptorTarget, property);
        if (descriptor) {
            break;
        }
        // Walk up the instance's prototype chain in case the property is declared
        // on a superclass.
        descriptorTarget = Object.getPrototypeOf(descriptorTarget);
    }
    return descriptor;
}
//# sourceMappingURL=observer.js.map
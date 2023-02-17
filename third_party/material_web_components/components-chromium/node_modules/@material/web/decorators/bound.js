/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
export function bound(target, propertyKey, methodDescriptor) {
    const descriptor = methodDescriptor || {
        configurable: true,
        enumerable: true,
        writable: true,
    };
    const memoizedBoundValues = new WeakMap();
    let get;
    let set;
    if (descriptor.get || descriptor.writable) {
        get = function () {
            const self = this; // Needed for closure conformance
            if (descriptor.get) {
                // Separate variables needed for closure conformance
                const getter = descriptor.get;
                const value = getter.call(self);
                return value.bind(self);
            }
            if (!memoizedBoundValues.has(self)) {
                const bound = (descriptor.value || self[propertyKey])?.bind(self);
                memoizedBoundValues.set(self, bound);
                return bound;
            }
            return memoizedBoundValues.get(self);
        };
    }
    if (descriptor.set || descriptor.writable) {
        set = function (value) {
            const self = this; // Needed for closure conformance
            value = value.bind(self);
            if (descriptor.set) {
                descriptor.set.call(self, value);
            }
            memoizedBoundValues.set(self, value);
        };
    }
    return {
        get,
        set,
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable,
    };
}
//# sourceMappingURL=bound.js.map
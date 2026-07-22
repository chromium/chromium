// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Map where the key is a subclass and the value is the mixin that produced the
// subclass. For example the following statement:
// const ClassB = MyMixin(ClassA);
// would produce an entry ClassB -> MyMixin
const appliedClassMixins = new WeakMap<Function, Function>();


// Cache used to avoid recreating mixin subclasses that have been encountered
// already. The key is a mixin and the value is a map mapping base classes to
// their generated subclasses. For example the following statement:
// const ClassA = MixinA(BaseElement);
// would produce an entry MixinA -> [BaseElement -> ClassA]
//
// A 2nd call as follows:
// const ClassB = MixinA(BaseElement);
// can reuse ClassA (such that ClassA === ClassB) instead of generating an
// unnecessary subclass.
const mixinApplications = new WeakMap<Function, WeakMap<Function, Function>>();

// Check if the mixin was previously applied.
function wasMixinPreviouslyApplied(
    mixin: Function, superClass: Function): boolean {
  let klass: Function|null = superClass;
  while (klass) {
    if (appliedClassMixins.get(klass) === mixin) {
      return true;
    }
    klass = Object.getPrototypeOf(klass);
  }
  return false;
}

// Make sure that Mixins are not applied more than once to the final class.
export function dedupingMixin<T extends Function>(mixin: T): T {
  return ((superClass: Function) => {
    if (wasMixinPreviouslyApplied(mixin, superClass)) {
      return superClass;
    }

    let cache = mixinApplications.get(mixin);
    if (!cache) {
      cache = new WeakMap<Function, Function>();
      mixinApplications.set(mixin, cache);
    }

    const cachedClass = cache.get(superClass);
    if (cachedClass) {
      return cachedClass;
    }

    const mixedClass = mixin(superClass);
    cache.set(superClass, mixedClass);
    appliedClassMixins.set(mixedClass, mixin);
    return mixedClass;
  }) as unknown as T;
}

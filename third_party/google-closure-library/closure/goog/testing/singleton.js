/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview This module simplifies testing code which uses stateful
 * singletons. `goog.testing.singleton.reset` resets all instances, so
 * next time when `getInstance` is called, a new instance is created.
 * It's recommended to reset the singletons in `tearDown` to prevent
 * interference between subsequent tests.
 *
 * The `goog.testing.singleton` functions expect that the goog.DEBUG flag
 * is enabled, and the tests are either uncompiled or compiled without renaming.
 */

goog.setTestOnly('goog.testing.singleton');
goog.provide('goog.testing.singleton');

goog.require('goog.singleton');

/**
 * Deletes all singleton instances, so `getInstance` will return a new
 * instance on next call.
 * @const
 */
goog.testing.singleton.resetAll = function() {
  'use strict';
  // Avoid concatenating arrays here - causes tests to perform poorly when there
  // are very large numbers of singletons to reset.
  const singletons1 = goog.getObjectByName('goog.instantiatedSingletons_');
  const singletons2 = goog.singleton.instantiatedSingletons;
  let ctor;
  while (ctor = singletons1.pop()) {
    goog.testing.singleton.reset(ctor);
  }
  while (ctor = singletons2.pop()) {
    goog.testing.singleton.reset(ctor);
  }
};

/**
 * Deletes a singleton's instance, so `getInstance` will return a new instance
 * on next call.
 * @param {function(new: Object)} singleton
 * @const
 * @suppress {missingProperties} 'instance_' isn't a property on any declared
 * type.
 */
goog.testing.singleton.reset = function(singleton) {
  'use strict';
  delete /** @type {?} */ (singleton).instance_;
};

/**
 * @deprecated Please use `goog.singleton.getInstance()`.
 */
goog.testing.singleton.addSingletonGetter = goog.addSingletonGetter;

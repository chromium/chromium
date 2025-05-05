/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A singleton interface for managing JavaScript code modules.
 */

goog.module('goog.loader.activeModuleManager');
goog.module.declareLegacyNamespace();

const AbstractModuleManager = goog.require('goog.loader.AbstractModuleManager');
const asserts = goog.require('goog.asserts');

/** @type {?AbstractModuleManager} */
let moduleManager = null;

/** @type {?function(): !AbstractModuleManager} */
let getDefault = null;

/** @type {!Array<function(!AbstractModuleManager)>} */
let configureFunctions = [];

/**
 * Applys a configuration function on moduleManager if it exists. Otherwise
 * store the configuration function inside of configureFunctions list so
 * that they can be applied when moduleManager is instantiated.
 * @param {function(!AbstractModuleManager)} configureFn
 */
function configure(configureFn) {
  if (moduleManager) {
    configureFn(moduleManager);
  } else {
    configureFunctions.push(configureFn);
  }
}

/**
 * Gets the active module manager, instantiating one if necessary.
 * @return {!AbstractModuleManager}
 */
function get() {
  if (!moduleManager && getDefault) {
    set(getDefault());
  }
  asserts.assert(
      moduleManager != null, 'The module manager has not yet been set.');
  return moduleManager;
}

/**
 * Sets the active module manager. This should never be used to override an
 * existing manager.
 *
 * @param {!AbstractModuleManager} newModuleManager
 */
function set(newModuleManager) {
  asserts.assert(
      moduleManager == null, 'The module manager cannot be redefined.');
  moduleManager = newModuleManager;
  configureFunctions.forEach(configureFn => {
    configureFn(/** @type {!AbstractModuleManager} */ (moduleManager));
  });
  configureFunctions = [];
}

/**
 * Stores a callback that will be used  to get an AbstractModuleManager instance
 * if set() is not called before the first get() call.
 * @param {function(): !AbstractModuleManager} fn
 */
function setDefault(fn) {
  getDefault = fn;
}

/**
 * Method called just before module code is loaded.
 * @param {string} id Identifier of the module.
 */
function beforeLoadModuleCode(id) {
  if (moduleManager) {
    moduleManager.beforeLoadModuleCode(id);
  }
}

/**
 * Records that the currently loading module was loaded. Also initiates loading
 * the next module if any module requests are queued. This method is called by
 * code that is generated and appended to each dynamic module's code at
 * compilation time.
 */
function setLoaded() {
  if (moduleManager) {
    moduleManager.setLoaded();
  }
}

/**
 * Initialize the module manager.
 * @param {string=} info A string representation of the module dependency
 *      graph, in the form: module1:dep1,dep2/module2:dep1,dep2 etc.
 *     Where depX is the base-36 encoded position of the dep in the module list.
 * @param {!Array<string>=} loadingModuleIds A list of moduleIds that
 *     are currently being loaded.
 */
function maybeInitialize(info, loadingModuleIds) {
  if (!moduleManager) {
    if (!getDefault) return;
    set(getDefault());
  }
  moduleManager.setAllModuleInfoString(info, loadingModuleIds);
}

/** Test-only method for removing the active module manager. */
const reset = function() {
  moduleManager = null;
  configureFunctions = [];
};

exports = {
  get,
  set,
  setDefault,
  beforeLoadModuleCode,
  setLoaded,
  maybeInitialize,
  reset,
  configure,
};

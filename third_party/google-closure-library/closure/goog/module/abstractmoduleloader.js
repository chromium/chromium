/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An interface for module loading.
 */

goog.module('goog.module.AbstractModuleLoader');
goog.module.declareLegacyNamespace();

const ModuleInfo = goog.requireType('goog.module.ModuleInfo');

/**
 * An interface that loads JavaScript modules.
 * @interface
 */
class AbstractModuleLoader {
  constructor() {
    /**
     * Whether or not the implementation supports extra edges.
     * @type {boolean|undefined}
     */
    this.supportsExtraEdges;
  }

  /**
   * Loads a list of JavaScript modules.
   *
   * @param {!Array<string>} ids The module ids in dependency order.
   * @param {!Object<string, !ModuleInfo>} moduleInfoMap A mapping
   *     from module id to ModuleInfo object.
   * @param {!AbstractModuleLoader.LoadOptions=} loadOptions
   */
  loadModules(ids, moduleInfoMap, loadOptions) {};


  /**
   * Pre-fetches a JavaScript module.
   *
   * @param {string} id The module id.
   * @param {!ModuleInfo} moduleInfo The module info.
   */
  prefetchModule(id, moduleInfo) {};
}

/**
 * A map of extra runtime module dependencies.
 * Since the polyfills for the ES6 Map/Set classes would cause a performance
 * regression, we are using plain Javascript objects to mimic their
 * functionality. The outer object will map a moduleId to another object, the
 * keys of which are the moduleIds of the modules it depends on: that is, if
 * `map['a']['b']` is true then module 'a' depends on module 'b'.
 * @typedef {!Object<!Object<boolean>>}
 */
AbstractModuleLoader.ExtraEdgesMap;

/**
 * Optional parameters for the loadModules method.
 * @record
 */
AbstractModuleLoader.LoadOptions = class {
  constructor() {
    /**
     * A map of extra runtime module dependencies.
     * @type {!AbstractModuleLoader.ExtraEdgesMap|undefined}
     */
    this.extraEdges;

    /**
     * Whether to bypass cache while loading the module.
     * @const {boolean|undefined}
     */
    this.forceReload;

    /**
     * The callback if module loading is an error.
     * @const {(function(?number): void)|undefined}
     */
    this.onError;

    /**
     * The callback if module loading is a success.
     * @const {(function(): void)|undefined}
     */
    this.onSuccess;

    /**
     * The callback if module loading times out.
     * @const {(function(): void)|undefined}
     */
    this.onTimeout;
  }
};

exports = AbstractModuleLoader;

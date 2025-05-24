/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The interface for module managers. The default implementation
 * is goog.module.ModuleManager.
 */

goog.provide('goog.loader.AbstractModuleManager');
goog.provide('goog.loader.AbstractModuleManager.CallbackType');

goog.require('goog.module.AbstractModuleLoader');
goog.require('goog.module.ModuleInfo');
goog.require('goog.module.ModuleLoadCallback');
goog.requireType('goog.html.TrustedResourceUrl');
goog.requireType('goog.module.BaseModule');



/**
 * The ModuleManager keeps track of all modules in the environment.
 * Since modules may not have their code loaded, we must keep track of them.
 * @abstract
 * @constructor
 * @struct
 */
goog.loader.AbstractModuleManager = function() {
  'use strict';
  /**
   * The module context needed for module initialization.
   * @private {?Object}
   */
  this.moduleContext_ = null;

  /**
   * A loader for the modules that implements loadModules(ids, moduleInfoMap,
   * opt_successFn, opt_errorFn, opt_timeoutFn, opt_forceReload) method.
   * @private {?goog.module.AbstractModuleLoader}
   */
  this.loader_ = null;
};


/**
 * The type of callbacks that can be registered with the module manager,.
 * @enum {string}
 */
goog.loader.AbstractModuleManager.CallbackType = {
  /**
   * Fired when an error has occurred.
   */
  ERROR: 'error',

  /**
   * Fired when it becomes idle and has no more module loads to process.
   */
  IDLE: 'idle',

  /**
   * Fired when it becomes active and has module loads to process.
   */
  ACTIVE: 'active',

  /**
   * Fired when it becomes idle and has no more user-initiated module loads to
   * process.
   */
  USER_IDLE: 'userIdle',

  /**
   * Fired when it becomes active and has user-initiated module loads to
   * process.
   */
  USER_ACTIVE: 'userActive'
};


/**
 * A non-HTTP status code indicating a corruption in loaded module.
 * This should be used by a ModuleLoader as a replacement for the HTTP code
 * given to the error handler function to indicated that the module was
 * corrupted.
 * This will set the forceReload flag on the loadModules method when retrying
 * module loading.
 * @type {number}
 */
goog.loader.AbstractModuleManager.CORRUPT_RESPONSE_STATUS_CODE = 8001;


/**
 * Sets the batch mode as enabled or disabled for the module manager.
 * @param {boolean} enabled Whether the batch mode is to be enabled or not.
 */
goog.loader.AbstractModuleManager.prototype.setBatchModeEnabled = function(
    enabled) {};


/**
 * Sets the concurrent loading mode as enabled or disabled for the module
 * manager. Requires a moduleloader implementation that supports concurrent
 * loads. The default {@see goog.module.ModuleLoader} does not.
 * @param {boolean} enabled
 */
goog.loader.AbstractModuleManager.prototype.setConcurrentLoadingEnabled =
    function(enabled) {};


/**
 * Sets the module info for all modules. Should only be called once.
 *
 * @param {!Object<!Array<string>>} infoMap An object that contains a mapping
 *    from module id (String) to list of required module ids (Array).
 */
goog.loader.AbstractModuleManager.prototype.setAllModuleInfo = function(
    infoMap) {};


/**
 * Sets the module info for all modules. Should only be called once. Also
 * marks modules that are currently being loaded.
 *
 * @param {string=} opt_info A string representation of the module dependency
 *      graph, in the form: module1:dep1,dep2/module2:dep1,dep2 etc.
 *     Where depX is the base-36 encoded position of the dep in the module list.
 * @param {!Array<string>=} opt_loadingModuleIds A list of moduleIds that
 *     are currently being loaded.
 */
goog.loader.AbstractModuleManager.prototype.setAllModuleInfoString = function(
    opt_info, opt_loadingModuleIds) {};


/**
 * Gets a module info object by id.
 * @param {string} id A module identifier.
 * @return {!goog.module.ModuleInfo} The module info.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.getModuleInfo = function(id) {};

/**
 * Register an extra runtime module dependency. After an extra edge is added,
 * any subsequent calls to load or loadMultiple will fetch toModule if the
 * fromModule was loaded.
 *
 * The mechanism for this is implementation dependent. If the implementation
 * does not support extra edges, it will throw an error.
 * @param {string} fromModule The dependent module of the extra edge.
 * @param {string} toModule The module dependency of the extra edge.
 */
goog.loader.AbstractModuleManager.prototype.addExtraEdge = function(
    fromModule, toModule) {
  'use strict';
  throw new Error('addExtraEdge is not implemented.');
};

/**
 * Remove an existing extra edge previously added by `addExtraEdge`.
 *
 * If the implementation does not support extra edges, it will throw an error.
 * @param {string} fromModule The dependent module of the extra edge.
 * @param {string} toModule The module dependency of the extra edge.
 */
goog.loader.AbstractModuleManager.prototype.removeExtraEdge = function(
    fromModule, toModule) {
  'use strict';
  throw new Error('removeExtraEdge is not implemented.');
};

/**
 * Sets the module uris.
 * @param {!Object<string, !Array<!goog.html.TrustedResourceUrl>>} moduleUriMap
 *     The map of id/uris pairs for each module.
 */
goog.loader.AbstractModuleManager.prototype.setModuleTrustedUris = function(
    moduleUriMap) {};


/**
 * Gets the application-specific module loader.
 * @return {?goog.module.AbstractModuleLoader} the loader.
 */
goog.loader.AbstractModuleManager.prototype.getLoader = function() {
  'use strict';
  return this.loader_;
};


/**
 * Sets the application-specific module loader.
 * @param {!goog.module.AbstractModuleLoader} loader
 */
goog.loader.AbstractModuleManager.prototype.setLoader = function(loader) {
  'use strict';
  this.loader_ = loader;
};


/**
 * Gets the module context to use to initialize the module.
 * @return {?Object} The context.
 */
goog.loader.AbstractModuleManager.prototype.getModuleContext = function() {
  'use strict';
  return this.moduleContext_;
};


/**
 * Sets the module context to use to initialize the module.
 * @param {!Object} context The context.
 */
goog.loader.AbstractModuleManager.prototype.setModuleContext = function(
    context) {
  'use strict';
  this.moduleContext_ = context;
};


/**
 * Determines if the ModuleManager is active
 * @return {boolean} TRUE iff the ModuleManager is active (i.e., not idle).
 */
goog.loader.AbstractModuleManager.prototype.isActive = function() {
  'use strict';
  return false;
};


/**
 * Determines if the ModuleManager is user active
 * @return {boolean} TRUE iff the ModuleManager is user active (i.e., not idle).
 */
goog.loader.AbstractModuleManager.prototype.isUserActive = function() {
  'use strict';
  return false;
};


/**
 * Preloads a module after a short delay.
 *
 * @param {string} id The id of the module to preload.
 * @param {number=} opt_timeout The number of ms to wait before adding the
 *     module id to the loading queue (defaults to 0 ms). Note that the module
 *     will be loaded asynchronously regardless of the value of this parameter.
 * @return {!IThenable}
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.preloadModule = function(
    id, opt_timeout) {};


/**
 * Prefetches a JavaScript module and its dependencies, which means that the
 * module will be downloaded, but not evaluated. To complete the module load,
 * the caller should also call load or execOnLoad after prefetching the module.
 *
 * @param {string} id The id of the module to prefetch.
 */
goog.loader.AbstractModuleManager.prototype.prefetchModule = function(id) {
  'use strict';
  throw new Error('prefetchModule is not implemented.');
};


/**
 * Records that the currently loading module was loaded. Also initiates loading
 * the next module if any module requests are queued. This method is called by
 * code that is generated and appended to each dynamic module's code at
 * compilation time.
 *
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.setLoaded = function() {};


/**
 * Gets whether a module is currently loading or in the queue, waiting to be
 * loaded.
 * @param {string} id A module id.
 * @return {boolean} TRUE iff the module is loading.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.isModuleLoading = function(id) {};


/**
 * Requests that a function be called once a particular module is loaded.
 * Client code can use this method to safely call into modules that may not yet
 * be loaded. For consistency, this method always calls the function
 * asynchronously -- even if the module is already loaded. Initiates loading of
 * the module if necessary, unless opt_noLoad is true.
 *
 * @param {string} moduleId A module id.
 * @param {!Function} fn Function to execute when the module has loaded.
 * @param {!Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 * @param {boolean=} opt_noLoad TRUE iff not to initiate loading of the module.
 * @param {boolean=} opt_userInitiated TRUE iff the loading of the module was
 *     user initiated.
 * @param {boolean=} opt_preferSynchronous TRUE iff the function should be
 *     executed synchronously if the module has already been loaded.
 * @return {!goog.module.ModuleLoadCallback} A callback wrapper that exposes
 *     an abort and execute method.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.execOnLoad = function(
    moduleId, fn, opt_handler, opt_noLoad, opt_userInitiated,
    opt_preferSynchronous) {};


/**
 * Loads a module, returning an IThenable for keeping track of the result.
 *
 * @param {string} moduleId A module id.
 * @param {boolean=} opt_userInitiated If the load is a result of a user action.
 * @return {!IThenable} A deferred object.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.load = function(
    moduleId, opt_userInitiated) {};


/**
 * Loads a list of modules, returning a map of IThenables for keeping track of
 * the results.
 *
 * @param {!Array<string>} moduleIds A list of module ids.
 * @param {boolean=} opt_userInitiated If the load is a result of a user action.
 * @return {!Object<string, !IThenable>} A mapping from id (String)
 *     to deferred objects that will callback or errback when the load for that
 *     id is finished.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.loadMultiple = function(
    moduleIds, opt_userInitiated) {};


/**
 * Method called just before module code is loaded.
 * @param {string} id Identifier of the module.
 * @abstract
 */
goog.loader.AbstractModuleManager.prototype.beforeLoadModuleCode = function(
    id) {};


/**
 * Register an initialization callback for the currently loading module. This
 * should only be called by script that is executed during the evaluation of
 * a module's javascript. This is almost equivalent to calling the function
 * inline, but ensures that all the code from the currently loading module
 * has been loaded. This makes it cleaner and more robust than calling the
 * function inline.
 *
 * If this function is called from the base module (the one that contains
 * the module manager code), the callback is held until #setAllModuleInfo
 * is called, or until #setModuleContext is called, whichever happens first.
 *
 * @param {!Function} fn A callback function that takes a single argument
 *    which is the module context.
 * @param {!Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 */
goog.loader.AbstractModuleManager.prototype.registerInitializationCallback =
    function(fn, opt_handler) {};


/**
 * Register a late initialization callback for the currently loading module.
 * Callbacks registered via this function are executed similar to
 * {@see registerInitializationCallback}, but they are fired after all
 * initialization callbacks are called.
 *
 * @param {!Function} fn A callback function that takes a single argument
 *    which is the module context.
 * @param {!Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 */
goog.loader.AbstractModuleManager.prototype.registerLateInitializationCallback =
    function(fn, opt_handler) {};


/**
 * Sets the constructor to use for the module object for the currently
 * loading module. The constructor should derive from
 * {@see goog.module.BaseModule}.
 * @param {function(new:goog.module.BaseModule)} fn The constructor function.
 */
goog.loader.AbstractModuleManager.prototype.setModuleConstructor = function(
    fn) {};


/**
 * The function to call if the module manager is in error.
 * @param {!goog.loader.AbstractModuleManager.CallbackType|!Array<
 *     !goog.loader.AbstractModuleManager.CallbackType>} types The callback
 *         type.
 * @param {!Function} fn The function to register as a callback.
 */
goog.loader.AbstractModuleManager.prototype.registerCallback = function(
    types, fn) {};

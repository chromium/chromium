/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The module loader for loading modules across the network.
 *
 * Browsers do not guarantee that scripts appended to the document
 * are executed in the order they are added. For production mode, we use
 * XHRs to load scripts, because they do not have this problem and they
 * have superior mechanisms for handling failure. However, XHR-evaled
 * scripts are harder to debug.
 *
 * In debugging mode, we use normal script tags. In order to make this work,
 * we load the scripts in serial: we do not execute script B to the document
 * until we are certain that script A is finished loading.
 */

goog.module('goog.module.ModuleLoader');

const AbstractModuleLoader = goog.require('goog.module.AbstractModuleLoader');
const BulkLoader = goog.require('goog.net.BulkLoader');
const EventHandler = goog.require('goog.events.EventHandler');
const EventId = goog.require('goog.events.EventId');
const EventTarget = goog.require('goog.events.EventTarget');
const EventType = goog.require('goog.net.EventType');
const GoogEvent = goog.require('goog.events.Event');
const ModuleInfo = goog.requireType('goog.module.ModuleInfo');
const SafeScript = goog.require('goog.html.SafeScript');
const TagName = goog.require('goog.dom.TagName');
const Timer = goog.require('goog.Timer');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const asserts = goog.require('goog.asserts');
const browser = goog.require('goog.labs.userAgent.browser');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const jsloader = goog.require('goog.net.jsloader');
const legacyconversions = goog.require('goog.html.legacyconversions');
const log = goog.require('goog.log');
const product = goog.require('goog.userAgent.product');
const safe = goog.require('goog.dom.safe');
const userAgent = goog.require('goog.userAgent');

/**
 * A class that loads JavaScript modules.
 * @constructor
 * @extends {EventTarget}
 * @implements {AbstractModuleLoader}
 */
function ModuleLoader() {
  ModuleLoader.base(this, 'constructor');

  /**
   * Event handler for managing handling events.
   * @type {!EventHandler<!ModuleLoader>}
   * @private
   */
  this.eventHandler_ = new EventHandler(this);
  this.registerDisposable(this.eventHandler_);

  /**
   * A map from module IDs to ModuleLoader.LoadStatus.
   * @type {!Object<!Array<string>, !ModuleLoader.LoadStatus>}
   * @private
   */
  this.loadingModulesStatus_ = {};
}
goog.inherits(ModuleLoader, EventTarget);


/**
 * A logger.
 * @type {?log.Logger}
 * @protected
 */
ModuleLoader.prototype.logger = log.getLogger('goog.module.ModuleLoader');


/**
 * Whether debug mode is enabled.
 * @type {boolean}
 * @private
 */
ModuleLoader.prototype.debugMode_ = false;


/**
 * Whether source url injection is enabled.
 * @type {boolean}
 * @private
 */
ModuleLoader.prototype.sourceUrlInjection_ = false;


/**
 * Whether to load modules with non-async script tags.
 * @type {boolean}
 * @private
 */
ModuleLoader.prototype.useScriptTags_ = false;


/**
 * @return {boolean} Whether sourceURL affects stack traces.
 */
ModuleLoader.supportsSourceUrlStackTraces = function() {
  return product.CHROME ||
      (browser.isFirefox() && browser.isVersionOrHigher('36'));
};


/**
 * @return {boolean} Whether sourceURL affects the debugger.
 */
ModuleLoader.supportsSourceUrlDebugger = function() {
  return product.CHROME || userAgent.GECKO;
};


/**
 * URLs have a browser-dependent max character limit. IE9-IE11 are the lowest
 * common denominators for what we support - with a limit of 4043:
 * https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers#31250734
 * If the URL constructed by the loader exceeds this limit, we will try to split
 * it into multiple requests.
 * TODO(user): Make this configurable since not all users care about IE.
 * @const {number}
 * @private
 */
ModuleLoader.URL_MAX_LENGTH_ = 4043;


/**
 * Error code for javascript syntax and network errors.
 * TODO(user): Detect more accurate error info.
 * @const {number}
 * @private
 */
ModuleLoader.SYNTAX_OR_NETWORK_ERROR_CODE_ = -1;



/**
 * @param {!TrustedResourceUrl} url The url to be loaded.
 * @return {!HTMLScriptElement}
 * @private
 */
ModuleLoader.createScriptElement_ = function(url) {
  const script = dom.createElement(TagName.SCRIPT);
  safe.setScriptSrc(script, url);

  // Set scriptElt.async = false to guarantee
  // that scripts are loaded in parallel but executed in the insertion order.
  // For more details, check
  // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/script
  script.async = false;
  return script;
};


/**
 * @param {!TrustedResourceUrl} url The url to be pre-loaded.
 * @return {!HTMLLinkElement}
 * @private
 */
ModuleLoader.createPreloadScriptElement_ = function(url) {
  const link = dom.createElement(TagName.LINK);
  safe.setLinkHrefAndRel(link, url, 'preload');
  link.as = 'script';

  // If CSP nonces are used, propagate them to dynamically created scripts.
  // This is necessary to allow nonce-based CSPs without 'strict-dynamic'.
  const nonce = safe.getScriptNonce();
  if (nonce) {
    link.setAttribute('nonce', nonce);
  }

  return link;
};


/**
 * Gets the debug mode for the loader.
 * @return {boolean} Whether the debug mode is enabled.
 */
ModuleLoader.prototype.getDebugMode = function() {
  return this.debugMode_;
};


/**
 * @param {boolean} useScriptTags Whether or not to use script tags
 *     (with async=false) for loading.
 */
ModuleLoader.prototype.setUseScriptTags = function(useScriptTags) {
  this.useScriptTags_ = useScriptTags;
};


/**
 * Gets whether we're using non-async script tags for loading.
 * @return {boolean} Whether or not we're using non-async script tags for
 *     loading.
 */
ModuleLoader.prototype.getUseScriptTags = function() {
  return this.useScriptTags_;
};


/**
 * Sets whether we're using non-async script tags for loading.
 * @param {boolean} debugMode Whether the debug mode is enabled.
 */
ModuleLoader.prototype.setDebugMode = function(debugMode) {
  this.debugMode_ = debugMode;
};


/**
 * When enabled, we will add a sourceURL comment to the end of all scripts
 * to mark their origin.
 *
 * On WebKit, stack traces will reflect the sourceURL comment, so this is
 * useful for debugging webkit stack traces in production.
 *
 * Notice that in debug mode, we will use source url injection + eval rather
 * then appending script nodes to the DOM, because the scripts will load far
 * faster.  (Appending script nodes is very slow, because we can't parallelize
 * the downloading and evaling of the script).
 *
 * The cost of appending sourceURL information is negligible when compared to
 * the cost of evaling the script. Almost all clients will want this on.
 *
 * TODO(nicksantos): Turn this on by default. We may want to turn this off
 * for clients that inject their own sourceURL.
 *
 * @param {boolean} enabled Whether source url injection is enabled.
 */
ModuleLoader.prototype.setSourceUrlInjection = function(enabled) {
  this.sourceUrlInjection_ = enabled;
};


/**
 * @return {boolean} Whether we're using source url injection.
 * @private
 */
ModuleLoader.prototype.usingSourceUrlInjection_ = function() {
  return this.sourceUrlInjection_ ||
      (this.getDebugMode() && ModuleLoader.supportsSourceUrlStackTraces());
};


/** @override */
ModuleLoader.prototype.loadModules = function(
    ids, moduleInfoMap, {forceReload, onError, onSuccess, onTimeout} = {}) {
  const loadStatus = this.loadingModulesStatus_[ids] ||
      ModuleLoader.LoadStatus.createForIds_(ids, moduleInfoMap);
  loadStatus.loadRequested = true;
  if (loadStatus.successFn && onSuccess) {
    // If there already exists a success function, chain it before the passed
    // success functon.
    loadStatus.successFn = functions.sequence(loadStatus.successFn, onSuccess);
  } else {
    loadStatus.successFn = onSuccess || loadStatus.successFn;
  }
  loadStatus.errorFn = onError || null;

  if (!this.loadingModulesStatus_[ids]) {
    // Modules were not prefetched.
    this.loadingModulesStatus_[ids] = loadStatus;
    this.downloadModules_(ids);
    // TODO(user): Need to handle timeouts in the module loading code.
  } else if (this.getUseScriptTags()) {
    // We started prefetching but we used <link rel="preload".../> tags, so we
    // rely on the browser to reconcile the (existing) prefetch request and the
    // script tag we're about to insert.
    this.downloadModules_(ids);
  } else if (loadStatus.responseTexts != null) {
    // Modules prefetch is complete.
    this.evaluateCode_(ids);
  }
  // Otherwise modules prefetch is in progress, and these modules will be
  // executed after the prefetch is complete.
};


/**
 * Evaluate the JS code.
 * @param {!Array<string>} moduleIds The module ids.
 * @private
 */
ModuleLoader.prototype.evaluateCode_ = function(moduleIds) {
  this.dispatchEvent(new ModuleLoader.RequestSuccessEvent(moduleIds));

  log.info(this.logger, 'evaluateCode ids:' + moduleIds);
  const loadStatus = this.loadingModulesStatus_[moduleIds];
  const uris = loadStatus.requestUris;
  const texts = loadStatus.responseTexts;
  let error = null;
  try {
    if (this.usingSourceUrlInjection_()) {
      for (let i = 0; i < uris.length; i++) {
        const script = legacyconversions.safeScriptFromString(
            texts[i] + ' //# sourceURL=' + uris[i]);
        goog.globalEval(SafeScript.unwrapTrustedScript(script));
      }
    } else {
      const script = legacyconversions.safeScriptFromString(texts.join('\n'));
      goog.globalEval(SafeScript.unwrapTrustedScript(script));
    }
  } catch (e) {
    error = e;
    // TODO(user): Consider throwing an exception here.
    log.warning(
        this.logger, 'Loaded incomplete code for module(s): ' + moduleIds, e);
  }

  this.dispatchEvent(new ModuleLoader.EvaluateCodeEvent(moduleIds));

  if (error) {
    this.handleErrorHelper_(
        moduleIds, loadStatus.errorFn, null /* status */, error);
  } else if (loadStatus.successFn) {
    loadStatus.successFn();
  }
  delete this.loadingModulesStatus_[moduleIds];
};


/**
 * Handles a successful response to a request for prefetch or load one or more
 * modules.
 *
 * @param {!BulkLoader} bulkLoader The bulk loader.
 * @param {!Array<string>} moduleIds The ids of the modules requested.
 * @private
 */
ModuleLoader.prototype.handleSuccess_ = function(bulkLoader, moduleIds) {
  log.info(this.logger, 'Code loaded for module(s): ' + moduleIds);

  const loadStatus = this.loadingModulesStatus_[moduleIds];
  loadStatus.responseTexts = bulkLoader.getResponseTexts();

  if (loadStatus.loadRequested) {
    this.evaluateCode_(moduleIds);
  }

  // NOTE: A bulk loader instance is used for loading a set of module ids.
  // Once these modules have been loaded successfully or in error the bulk
  // loader should be disposed as it is not needed anymore. A new bulk loader
  // is instantiated for any new modules to be loaded. The dispose is called
  // on a timer so that the bulkloader has a chance to release its
  // objects.
  Timer.callOnce(bulkLoader.dispose, 5, bulkLoader);
};


/** @override */
ModuleLoader.prototype.prefetchModule = function(id, moduleInfo) {
  // Do not prefetch in debug mode
  if (this.getDebugMode()) {
    return;
  }
  log.info(this.logger, `Prefetching module: ${id}`);
  let loadStatus = this.loadingModulesStatus_[[id]];
  if (loadStatus) {
    return;
  }
  const moduleInfoMap = {};
  moduleInfoMap[id] = moduleInfo;
  loadStatus = ModuleLoader.LoadStatus.createForIds_([id], moduleInfoMap);
  this.loadingModulesStatus_[[id]] = loadStatus;
  if (this.getUseScriptTags()) {
    const links = [];
    const insertPos = document.head || document.documentElement;
    for (let i = 0; i < loadStatus.trustedRequestUris.length; i++) {
      const link = ModuleLoader.createPreloadScriptElement_(
          loadStatus.trustedRequestUris[i]);
      links.push(link);
      insertPos.insertBefore(link, insertPos.firstChild);
    }
    loadStatus.successFn = () => {
      for (let i = 0; i < links.length; i++) {
        const link = links[i];
        dom.removeNode(link);
      }
    };
  } else {
    this.downloadModules_([id]);
  }
};


/**
 * Downloads a list of JavaScript modules.
 *
 * @param {!Array<string>} ids The module ids in dependency order.
 * @private
 */
ModuleLoader.prototype.downloadModules_ = function(ids) {
  const debugMode = this.getDebugMode();
  const sourceUrlInjection = this.usingSourceUrlInjection_();
  const useScriptTags = this.getUseScriptTags();
  if ((debugMode + sourceUrlInjection + useScriptTags) > 1) {
    const effectiveFlag = useScriptTags ?
        'useScriptTags' :
        (debugMode && !sourceUrlInjection) ? 'debug' : 'sourceUrlInjection';
    log.warning(
        this.logger,
        `More than one of debugMode (set to ${debugMode}), ` +
            `useScriptTags (set to ${useScriptTags}), ` +
            `and sourceUrlInjection (set to ${sourceUrlInjection}) ` +
            `is enabled. Proceeding with download as if ` +
            `${effectiveFlag} is set to true and the rest to false.`);
  }
  const loadStatus = asserts.assert(this.loadingModulesStatus_[ids]);

  if (useScriptTags) {
    this.loadWithNonAsyncScriptTag_(loadStatus, ids);
  } else if (debugMode && !sourceUrlInjection) {
    // In debug mode use <script> tags rather than XHRs to load the files.
    // This makes it possible to debug and inspect stack traces more easily.
    // It's also possible to use it to load JavaScript files that are hosted on
    // another domain.
    // The scripts need to load serially, so this is much slower than parallel
    // script loads with source url injection.
    jsloader.safeLoadMany(loadStatus.trustedRequestUris);
  } else {
    log.info(
        this.logger,
        'downloadModules ids:' + ids + ' uris:' + loadStatus.requestUris);

    const bulkLoader = new BulkLoader(loadStatus.requestUris);

    const eventHandler = this.eventHandler_;
    eventHandler.listen(
        bulkLoader, EventType.SUCCESS,
        goog.bind(this.handleSuccess_, this, bulkLoader, ids));
    eventHandler.listen(
        bulkLoader, EventType.ERROR,
        goog.bind(this.handleError_, this, bulkLoader, ids));
    bulkLoader.load();
  }
};


/**
 * Downloads a list of script URIS using <script async=false.../>, which
 * guarantees executuion order.
 * @param {!ModuleLoader.LoadStatus} loadStatus The load status
 *     object for this module-load.
 *  @param {!Array<string>} ids The module ids in dependency order.
 * @private
 */
ModuleLoader.prototype.loadWithNonAsyncScriptTag_ = function(loadStatus, ids) {
  log.info(this.logger, `Loading initiated for: ${ids}`);
  if (loadStatus.trustedRequestUris.length == 0) {
    if (loadStatus.successFn) {
      loadStatus.successFn();
      return;
    }
  }

  // We'll execute the success callback when the last script enqueed reaches
  // onLoad.
  let lastScript = null;
  const insertPos = document.head || document.documentElement;

  for (let i = 0; i < loadStatus.trustedRequestUris.length; i++) {
    const url = loadStatus.trustedRequestUris[i];
    const urlLength = loadStatus.requestUris[i].length;
    asserts.assert(
        urlLength <= ModuleLoader.URL_MAX_LENGTH_,
        `Module url length is ${urlLength}, which is greater than limit of ` +
            `${ModuleLoader.URL_MAX_LENGTH_}. This should never ` +
            `happen.`);

    const scriptElement = ModuleLoader.createScriptElement_(url);

    scriptElement.onload = () => {
      scriptElement.onload = null;
      scriptElement.onerror = null;
      dom.removeNode(scriptElement);
      if (scriptElement == lastScript) {
        log.info(this.logger, `Loading complete for: ${ids}`);
        lastScript = null;
        if (loadStatus.successFn) {
          loadStatus.successFn();
        }
      }
    };

    scriptElement.onerror = () => {
      log.error(this.logger, `Network error when loading module(s): ${ids}`);
      scriptElement.onload = null;
      scriptElement.onerror = null;
      dom.removeNode(scriptElement);
      this.handleErrorHelper_(
          ids, loadStatus.errorFn, ModuleLoader.SYNTAX_OR_NETWORK_ERROR_CODE_);
      if (lastScript == scriptElement) {
        lastScript = null;
      } else {
        log.error(
            this.logger,
            `Dependent requests were made in parallel with failed request ` +
                `for module(s) "${ids}". Non-recoverable out-of-order ` +
                `execution may occur.`);
      }
    };
    lastScript = scriptElement;
    insertPos.insertBefore(scriptElement, insertPos.firstChild);
  }
};


/**
 * Handles an error during a request for one or more modules.
 * @param {!BulkLoader} bulkLoader The bulk loader.
 * @param {!Array<string>} moduleIds The ids of the modules requested.
 * @param {!BulkLoader.LoadErrorEvent} event The load error event.
 * @private
 */
ModuleLoader.prototype.handleError_ = function(bulkLoader, moduleIds, event) {
  const loadStatus = this.loadingModulesStatus_[moduleIds];
  // The bulk loader doesn't cancel other requests when a request fails. We will
  // delete the loadStatus in the first failure, so it will be undefined in
  // subsequent errors.
  if (loadStatus) {
    delete this.loadingModulesStatus_[moduleIds];
    this.handleErrorHelper_(moduleIds, loadStatus.errorFn, event.status);
  }

  // NOTE: A bulk loader instance is used for loading a set of module ids. Once
  // these modules have been loaded successfully or in error the bulk loader
  // should be disposed as it is not needed anymore. A new bulk loader is
  // instantiated for any new modules to be loaded. The dispose is called
  // on another thread so that the bulkloader has a chance to release its
  // objects.
  Timer.callOnce(bulkLoader.dispose, 5, bulkLoader);
};


/**
 * Handles an error during a request for one or more modules.
 * @param {!Array<string>} moduleIds The ids of the modules requested.
 * @param {?function(?number)} errorFn The function to call on failure.
 * @param {?number} status The response status.
 * @param {!Error=} opt_error The error encountered, if available.
 * @private
 */
ModuleLoader.prototype.handleErrorHelper_ = function(
    moduleIds, errorFn, status, opt_error) {
  this.dispatchEvent(
      new ModuleLoader.RequestErrorEvent(moduleIds, status, opt_error));

  log.warning(this.logger, 'Request failed for module(s): ' + moduleIds);

  if (errorFn) {
    errorFn(status);
  }
};


/**
 * Events dispatched by the ModuleLoader.
 * @const
 */
ModuleLoader.EventType = {
  /**
   * @const {!EventId<
   *     !ModuleLoader.EvaluateCodeEvent>} Called after the code for
   *     a module is evaluated.
   */
  EVALUATE_CODE: new EventId(events.getUniqueId('evaluateCode')),

  /**
   * @const {!EventId<
   *     !ModuleLoader.RequestSuccessEvent>} Called when the
   *     BulkLoader finishes successfully.
   */
  REQUEST_SUCCESS: new EventId(events.getUniqueId('requestSuccess')),

  /**
   * @const {!EventId<
   *     !ModuleLoader.RequestErrorEvent>} Called when the
   *     BulkLoader fails, or code loading fails.
   */
  REQUEST_ERROR: new EventId(events.getUniqueId('requestError'))
};



/**
 * @param {!Array<string>} moduleIds The ids of the modules being evaluated.
 * @constructor
 * @extends {GoogEvent}
 * @final
 * @protected
 */
ModuleLoader.EvaluateCodeEvent = function(moduleIds) {
  ModuleLoader.EvaluateCodeEvent.base(
      this, 'constructor', ModuleLoader.EventType.EVALUATE_CODE);

  /**
   * @type {!Array<string>}
   */
  this.moduleIds = moduleIds;
};
goog.inherits(ModuleLoader.EvaluateCodeEvent, GoogEvent);



/**
 * @param {!Array<string>} moduleIds The ids of the modules being evaluated.
 * @constructor
 * @extends {GoogEvent}
 * @final
 * @protected
 */
ModuleLoader.RequestSuccessEvent = function(moduleIds) {
  ModuleLoader.RequestSuccessEvent.base(
      this, 'constructor', ModuleLoader.EventType.REQUEST_SUCCESS);

  /**
   * @type {!Array<string>}
   */
  this.moduleIds = moduleIds;
};
goog.inherits(ModuleLoader.RequestSuccessEvent, GoogEvent);



/**
 * @param {!Array<string>} moduleIds The ids of the modules being evaluated.
 * @param {?number} status The response status.
 * @param {!Error=} opt_error The error encountered, if available.
 * @constructor
 * @extends {GoogEvent}
 * @final
 * @protected
 */
ModuleLoader.RequestErrorEvent = function(moduleIds, status, opt_error) {
  ModuleLoader.RequestErrorEvent.base(
      this, 'constructor', ModuleLoader.EventType.REQUEST_ERROR);

  /**
   * @type {?Array<string>}
   */
  this.moduleIds = moduleIds;

  /** @type {?number} */
  this.status = status;

  /** @type {?Error} */
  this.error = opt_error || null;
};
goog.inherits(ModuleLoader.RequestErrorEvent, GoogEvent);



/**
 * A class that keeps the state of the module during the loading process. It is
 * used to save loading information between modules download and evaluation.
 *  @param {!Array<!TrustedResourceUrl>} trustedRequestUris the uris
 containing the modules implementing ids.

 * @constructor
 * @final
 */
ModuleLoader.LoadStatus = function(trustedRequestUris) {
  /**
   * The request uris.
   * @final {!Array<string>}
   */
  this.requestUris = trustedRequestUris.map(TrustedResourceUrl.unwrap);

  /**
   * A TrustedResourceUrl version of `this.requestUris`
   * @final {!Array<!TrustedResourceUrl>}
   */
  this.trustedRequestUris = trustedRequestUris;

  /**
   * The response texts.
   * @type {?Array<string>}
   */
  this.responseTexts = null;

  /**
   * Whether loadModules was called for the set of modules referred by this
   * status.
   * @type {boolean}
   */
  this.loadRequested = false;

  /**
   * Success callback.
   * @type {?function()}
   */
  this.successFn = null;

  /**
   * Error callback.
   * @type {?function(?number)}
   */
  this.errorFn = null;
};


/**
 * Creates a `LoadStatus` object for tracking state during the loading of the
 * modules indexed in `ids`.
 *
 * @param {?Array<string>} ids the ids for this module load in dependency
 *   order.
 * @param {!Object<string, !ModuleInfo>} moduleInfoMap A mapping
 *     from module id to ModuleInfo object.
 * @return {!ModuleLoader.LoadStatus}
 * @private
 */
ModuleLoader.LoadStatus.createForIds_ = function(ids, moduleInfoMap) {
  if (!ids) {
    return new ModuleLoader.LoadStatus([]);
  }
  const trustedRequestUris = [];
  for (let i = 0; i < ids.length; i++) {
    googArray.extend(trustedRequestUris, moduleInfoMap[ids[i]].getUris());
  }
  return new ModuleLoader.LoadStatus(trustedRequestUris);
};


exports = ModuleLoader;

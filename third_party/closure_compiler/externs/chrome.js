//    SSSSSSSSSSSSSSS TTTTTTTTTTTTTTTTTTTTTTT     OOOOOOOOO     PPPPPPPPPPPPPPPPP
//  SS:::::::::::::::ST:::::::::::::::::::::T   OO:::::::::OO   P::::::::::::::::P
// S:::::SSSSSS::::::ST:::::::::::::::::::::T OO:::::::::::::OO P::::::PPPPPP:::::P
// S:::::S     SSSSSSST:::::TT:::::::TT:::::TO:::::::OOO:::::::OPP:::::P     P:::::P
// S:::::S            TTTTTT  T:::::T  TTTTTTO::::::O   O::::::O  P::::P     P:::::P
// S:::::S                    T:::::T        O:::::O     O:::::O  P::::P     P:::::P
//  S::::SSSS                                                     P::::PPPPPP:::::P
//   SS::::::SSSSS       This file is generated. To update it,    P:::::::::::::PP
//     SSS::::::::SS          run roll_closure_compiler.          P::::PPPPPPPPP
//        SSSSSS::::S                                             P::::P
//             S:::::S        T:::::T        O:::::O     O:::::O  P::::P
//             S:::::S        T:::::T        O::::::O   O::::::O  P::::P
// SSSSSSS     S:::::S      TT:::::::TT      O:::::::OOO:::::::OPP::::::PP
// S::::::SSSSSS:::::S      T:::::::::T       OO:::::::::::::OO P::::::::P
// S:::::::::::::::SS       T:::::::::T         OO:::::::::OO   P::::::::P
//  SSSSSSSSSSSSSSS         TTTTTTTTTTT           OOOOOOOOO     PPPPPPPPPP
/*
 * Copyright 2013 The Closure Compiler Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Definitions for globals in Chrome.  This file describes the
 * externs API for the chrome.* object when running in a normal browser
 * context.  For APIs available in Chrome Extensions, see chrome_extensions.js
 * in this directory.
 * @externs
 */


/**
 * namespace
 * @const
 */
var chrome = {};


/**
 * @see http://developer.chrome.com/apps/runtime.html#type-Port
 * @constructor
 */
function Port() {}


/** @type {string} */
Port.prototype.name;


/** @type {!ChromeEvent} */
Port.prototype.onDisconnect;


/** @type {!ChromeEvent} */
Port.prototype.onMessage;


/** @type {!MessageSender|undefined} */
Port.prototype.sender;


/**
 * @param {*} obj Message object.
 * @return {undefined}
 */
Port.prototype.postMessage = function(obj) {};


/** @return {undefined} */
Port.prototype.disconnect = function() {};


/**
 * Base event type without listener methods.
 *
 * This interface exists for event interfaces whose addListeners() method takes
 * more than one parameter. Those interfaces must inherit from this one, so they
 * can supply their own custom listener method declarations.
 *
 * Event interfaces whose addListeners() method takes just one parameter should
 * inherit from ChromeBaseEvent instead. It extends this interface.
 *
 * @see https://developer.chrome.com/extensions/events
 * @interface
 */
function ChromeBaseEventNoListeners() {}


/**
 * @param {!Array<!Rule>} rules
 * @param {function(!Array<!Rule>): void=} callback
 * @see https://developer.chrome.com/extensions/events#method-Event-addRules
 */
ChromeBaseEventNoListeners.prototype.addRules = function(rules, callback) {};


/**
 * Returns currently registered rules.
 *
 * NOTE: The API allows the first argument to be omitted.
 *     That cannot be correctly represented here, so we end up incorrectly
 *     allowing 2 callback arguments.
 * @param {!Array<string>|function(!Array<!Rule>): void} ruleIdentifiersOrCb
 * @param {function(!Array<!Rule>): void=} callback
 * @see https://developer.chrome.com/extensions/events#method-Event-getRules
 */
ChromeBaseEventNoListeners.prototype.getRules =
    function(ruleIdentifiersOrCb, callback) {};


/**
 * Removes currently registered rules.
 *
 * NOTE: The API allows the either or both arguments to be omitted.
 *     That cannot be correctly represented here, so we end up incorrectly
 *     allowing 2 callback arguments.
 * @param {(!Array<string>|function(): void)=} ruleIdentifiersOrCb
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/events#method-Event-removeRules
 */
ChromeBaseEventNoListeners.prototype.removeRules =
    function(ruleIdentifiersOrCb, callback) {};


/**
 * @see https://developer.chrome.com/extensions/events#type-Rule
 * @record
 */
function Rule() {}


/** @type {string|undefined} */
Rule.prototype.id;


/** @type {!Array<string>|undefined} */
Rule.prototype.tags;


/** @type {!Array<*>} */
Rule.prototype.conditions;


/** @type {!Array<*>} */
Rule.prototype.actions;


/** @type {number|undefined} */
Rule.prototype.priority;


/**
 * @see https://developer.chrome.com/extensions/events#type-UrlFilter
 * @record
 */
function UrlFilter() {}


/** @type {string|undefined} */
UrlFilter.prototype.hostContains;


/** @type {string|undefined} */
UrlFilter.prototype.hostEquals;


/** @type {string|undefined} */
UrlFilter.prototype.hostPrefix;


/** @type {string|undefined} */
UrlFilter.prototype.hostSuffix;


/** @type {string|undefined} */
UrlFilter.prototype.pathContains;


/** @type {string|undefined} */
UrlFilter.prototype.pathEquals;


/** @type {string|undefined} */
UrlFilter.prototype.pathPrefix;


/** @type {string|undefined} */
UrlFilter.prototype.pathSuffix;


/** @type {string|undefined} */
UrlFilter.prototype.queryContains;


/** @type {string|undefined} */
UrlFilter.prototype.queryEquals;


/** @type {string|undefined} */
UrlFilter.prototype.queryPrefix;


/** @type {string|undefined} */
UrlFilter.prototype.querySuffix;


/** @type {string|undefined} */
UrlFilter.prototype.urlContains;


/** @type {string|undefined} */
UrlFilter.prototype.urlEquals;


/** @type {string|undefined} */
UrlFilter.prototype.urlMatches;


/** @type {string|undefined} */
UrlFilter.prototype.originAndPathMatches;


/** @type {string|undefined} */
UrlFilter.prototype.urlPrefix;


/** @type {string|undefined} */
UrlFilter.prototype.urlSuffix;


/** @type {!Array<string>|undefined} */
UrlFilter.prototype.schemes;


/** @type {!Array<(number|!Array<number>)>|undefined} */
UrlFilter.prototype.ports;


/**
 * Base event type from which all others inherit.
 *
 * LISTENER must be a function type that returns void.
 *
 * @see https://developer.chrome.com/extensions/events
 * @interface
 * @extends {ChromeBaseEventNoListeners}
 * @template LISTENER
 */
function ChromeBaseEvent() {}


/**
 * @param {LISTENER} callback
 * @return {undefined}
 * @see https://developer.chrome.com/extensions/events#method-Event-addListener
 */
ChromeBaseEvent.prototype.addListener = function(callback) {};


/**
 * @param {LISTENER} callback
 * @return {undefined}
 * @see https://developer.chrome.com/extensions/events#method-Event-removeListener
 */
ChromeBaseEvent.prototype.removeListener = function(callback) {};


/**
 * @param {LISTENER} callback
 * @return {boolean}
 * @see https://developer.chrome.com/extensions/events#method-Event-hasListener
 */
ChromeBaseEvent.prototype.hasListener = function(callback) {};


/**
 * @return {boolean}
 * @see https://developer.chrome.com/extensions/events#method-Event-hasListeners
 */
ChromeBaseEvent.prototype.hasListeners = function() {};


/**
 * Event whose listeners take unspecified parameters.
 *
 * TODO(bradfordcsmith): Definitions using this type are failing to provide
 *     information about the parameters that will actually be supplied to the
 *     listener and should be updated to use a more specific event type.
 * @see https://developer.chrome.com/extensions/events
 * @interface
 * @extends {ChromeBaseEvent<!Function>}
 */
function ChromeEvent() {}


/**
 * Event whose listeners take no parameters.
 *
 * @see https://developer.chrome.com/extensions/events
 * @interface
 * @extends {ChromeBaseEvent<function()>}
 */
function ChromeVoidEvent() {}


/**
 * Event whose listeners take a string parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(string)>}
 */
function ChromeStringEvent() {}


/**
 * Event whose listeners take a boolean parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(boolean)>}
 */
function ChromeBooleanEvent() {}


/**
 * Event whose listeners take a number parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(number)>}
 */
function ChromeNumberEvent() {}


/**
 * Event whose listeners take an Object parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!Object)>}
 */
function ChromeObjectEvent() {}


/**
 * Event whose listeners take a string array parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!Array<string>)>}
 */
function ChromeStringArrayEvent() {}


/**
 * Event whose listeners take two strings as parameters.
 * @interface
 * @extends {ChromeBaseEvent<function(string, string)>}
 */
function ChromeStringStringEvent() {}


/**
 * @see http://developer.chrome.com/extensions/runtime.html#type-MessageSender
 * @constructor
 */
function MessageSender() {}


/** @type {!Tab|undefined} */
MessageSender.prototype.tab;


/** @type {number|undefined} */
MessageSender.prototype.frameId;


/** @type {string|undefined} */
MessageSender.prototype.id;


/** @type {string|undefined} */
MessageSender.prototype.url;


/** @type {string|undefined} */
MessageSender.prototype.nativeApplication;


/** @type {string|undefined} */
MessageSender.prototype.tlsChannelId;


/** @type {string|undefined} */
MessageSender.prototype.origin;


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/tabs#type-MutedInfoReason
 */
var MutedInfoReason = {
  USER: '',
  CAPTURE: '',
  EXTENSION: '',
};


/**
 * @see https://developer.chrome.com/extensions/tabs#type-MutedInfo
 * @constructor
 */
var MutedInfo = function() {};


/** @type {boolean} */
MutedInfo.prototype.muted;


/** @type {!MutedInfoReason|string|undefined} */
MutedInfo.prototype.reason;


/** @type {string|undefined} */
MutedInfo.prototype.extensionId;




/**
 * @see https://developer.chrome.com/extensions/tabs#type-Tab
 * @constructor
 */
function Tab() {}


// TODO: Make this field optional once dependent projects have been updated.
/** @type {number} */
Tab.prototype.id;


/** @type {number} */
Tab.prototype.index;


/** @type {number} */
Tab.prototype.windowId;


// TODO: Make this field optional once dependent projects have been updated.
/** @type {number} */
Tab.prototype.openerTabId;


/** @type {boolean} */
Tab.prototype.highlighted;


/** @type {boolean} */
Tab.prototype.active;


/** @type {boolean} */
Tab.prototype.pinned;


/** @type {boolean|undefined} */
Tab.prototype.audible;


/** @type {boolean} */
Tab.prototype.discarded;


/** @type {boolean} */
Tab.prototype.autoDiscardable;


/** @type {!MutedInfo|undefined} */
Tab.prototype.mutedInfo;


// TODO: Make this field optional once dependent projects have been updated.
/** @type {string} */
Tab.prototype.url;


/** @type {string|undefined} */
Tab.prototype.pendingUrl;


// TODO: Make this field optional once dependent projects have been updated.
/** @type {string} */
Tab.prototype.title;


// TODO: Make this field optional once dependent projects have been updated.
/** @type {string} */
Tab.prototype.favIconUrl;


// TODO: Make this field optional once dependent projects have been updated.
/** @type {string} */
Tab.prototype.status;


/** @type {boolean} */
Tab.prototype.incognito;


/** @type {number|undefined} */
Tab.prototype.width;


/** @type {number|undefined} */
Tab.prototype.height;


/** @type {string|undefined} */
Tab.prototype.sessionId;


/** @const */
chrome.app = {};

/**
 * @see https://developer.chrome.com/webstore/inline_installation#already-installed
 * @type {boolean}
 */
chrome.app.isInstalled;

/**
 * @const
 * @see https://developer.chrome.com/apps/webstore
 */
chrome.webstore = {};


/**
 * @param {string|function()|function(string, string=)=}
 *     opt_urlOrSuccessCallbackOrFailureCallback Either the URL to install or
 *     the succcess callback taking no arg or the failure callback taking an
 *     error string arg.
 * @param {function()|function(string, string=)=}
 *     opt_successCallbackOrFailureCallback Either the succcess callback taking
 *     no arg or the failure callback taking an error string arg.
 * @param {function(string, string=)=} opt_failureCallback The failure callback.
 * @return {undefined}
 */
chrome.webstore.install = function(
    opt_urlOrSuccessCallbackOrFailureCallback,
    opt_successCallbackOrFailureCallback,
    opt_failureCallback) {};


/** @type {!ChromeStringEvent} */
chrome.webstore.onInstallStageChanged;


/** @type {!ChromeNumberEvent} */
chrome.webstore.onDownloadProgress;


/**
 * Returns an object representing current load times. Note that the properties
 * on the object do not change and the function must be called again to get
 * up-to-date data.
 *
 * @return {!ChromeLoadTimes}
 */
chrome.loadTimes = function() {};



/**
 * The data object given by chrome.loadTimes().
 * @constructor
 */
function ChromeLoadTimes() {}


/** @type {number} */
ChromeLoadTimes.prototype.requestTime;


/** @type {number} */
ChromeLoadTimes.prototype.startLoadTime;


/** @type {number} */
ChromeLoadTimes.prototype.commitLoadTime;


/** @type {number} */
ChromeLoadTimes.prototype.finishDocumentLoadTime;


/** @type {number} */
ChromeLoadTimes.prototype.finishLoadTime;


/** @type {number} */
ChromeLoadTimes.prototype.firstPaintTime;


/** @type {number} */
ChromeLoadTimes.prototype.firstPaintAfterLoadTime;


/** @type {number} */
ChromeLoadTimes.prototype.navigationType;


/**
 * True iff the resource was fetched over SPDY.
 * @type {boolean}
 */
ChromeLoadTimes.prototype.wasFetchedViaSpdy;


/** @type {boolean} */
ChromeLoadTimes.prototype.wasNpnNegotiated;


/** @type {string} */
ChromeLoadTimes.prototype.npnNegotiatedProtocol;


/** @type {boolean} */
ChromeLoadTimes.prototype.wasAlternateProtocolAvailable;


/** @type {string} */
ChromeLoadTimes.prototype.connectionInfo;


/**
 * @param {string|!ArrayBuffer|!Object} message
 * @see https://developers.google.com/native-client/devguide/tutorial
 * @return {undefined}
 */
HTMLEmbedElement.prototype.postMessage = function(message) {};

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1085633): Inline enums need to be cleaned up in the source
// specification before we can autogenerate this.

/**
 * @fileoverview Externs generated from namespace: webview
 * @externs
 */

/**
 * @typedef {{
 *   since: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-ClearDataOptions
 */
var ClearDataOptions;

/**
 * @typedef {{
 *   appcache: (boolean|undefined),
 *   cookies: (boolean|undefined),
 *   fileSystems: (boolean|undefined),
 *   indexedDB: (boolean|undefined),
 *   localStorage: (boolean|undefined),
 *   webSQL: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-ClearDataTypeSet
 */
var ClearDataTypeSet;

/**
 * @typedef {{
 *   code: (string|undefined),
 *   file: (string|undefined)
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-InjectDetails
 */
var InjectDetails;

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-ContentWindow
 */
function ContentWindow() {}

/**
 * @param {?} message
 * @param {string} targetOrigin
 */
ContentWindow.prototype.postMessage = function(message, targetOrigin) {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-DialogController
 */
function DialogController() {}

/**
 * @param {string} response
 */
DialogController.prototype.ok = function(response) {};

DialogController.prototype.cancel = function() {};

/**
 * @typedef {{
 *   numberOfMatches: number,
 *   activeMatchOrdinal: number,
 *   selectionRect: SelectionRect,
 *   canceled: boolean
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-FindCallbackResults
 */
var FindCallbackResults;

/**
 * @typedef {{
 *   backward: (boolean|undefined),
 *   matchCase: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-FindOptions
 */
var FindOptions;

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-NewWindow
 */
function NewWindow() {}

/**
 * @param {!Object} webview
 */
NewWindow.prototype.attach = function(webview) {};

NewWindow.prototype.discard = function() {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-MediaPermissionRequest
 */
function MediaPermissionRequest() {}

/** @type {string} */
MediaPermissionRequest.prototype.url;

MediaPermissionRequest.prototype.allow = function() {};

MediaPermissionRequest.prototype.deny = function() {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-GeolocationPermissionRequest
 */
function GeolocationPermissionRequest() {}

/** @type {string} */
GeolocationPermissionRequest.prototype.url;

GeolocationPermissionRequest.prototype.allow = function() {};

GeolocationPermissionRequest.prototype.deny = function() {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-PointerLockPermissionRequest
 */
function PointerLockPermissionRequest() {}

/** @type {boolean} */
PointerLockPermissionRequest.prototype.userGesture;

/** @type {boolean} */
PointerLockPermissionRequest.prototype.lastUnlockedBySelf;

/** @type {string} */
PointerLockPermissionRequest.prototype.url;

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-DownloadPermissionRequest
 */
function DownloadPermissionRequest() {}

/** @type {string} */
DownloadPermissionRequest.prototype.requestMethod;

/** @type {string} */
DownloadPermissionRequest.prototype.url;

DownloadPermissionRequest.prototype.allow = function() {};

DownloadPermissionRequest.prototype.deny = function() {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-FileSystemPermissionRequest
 */
function FileSystemPermissionRequest() {}

/** @type {string} */
FileSystemPermissionRequest.prototype.url;

FileSystemPermissionRequest.prototype.allow = function() {};

FileSystemPermissionRequest.prototype.deny = function() {};

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-LoadPluginPermissionRequest
 */
function LoadPluginPermissionRequest() {}

/** @type {string} */
LoadPluginPermissionRequest.prototype.identifier;

/** @type {string} */
LoadPluginPermissionRequest.prototype.name;

LoadPluginPermissionRequest.prototype.allow = function() {};

LoadPluginPermissionRequest.prototype.deny = function() {};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
var SelectionRect;

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-WebRequestEventInterface
 */
function WebRequestEventInterface() {}

/** @type {!WebRequestOptionallySynchronousEvent} */
WebRequestEventInterface.prototype.onBeforeRequest;

/** @type {!WebRequestOptionallySynchronousEvent} */
WebRequestEventInterface.prototype.onBeforeSendHeaders;

/** @type {!WebRequestEvent} */
WebRequestEventInterface.prototype.onCompleted;

/** @type {!WebRequestEvent} */
WebRequestEventInterface.prototype.onBeforeRedirect;

/** @type {!WebRequestOnErrorOccurredEvent} */
WebRequestEventInterface.prototype.onErrorOccurred;

/** @type {!WebRequestOptionallySynchronousEvent} */
WebRequestEventInterface.prototype.onHeadersReceived;

/**
 * @constructor
 * @extends {HTMLIFrameElement}
 */
function WebView() {}

/**
 * @type {ContentWindow}
 * @see https://developer.chrome.com/apps/tags/webview#property-contentWindow
 */
WebView.prototype.contentWindow;

/**
 * @type {!WebRequestEventInterface}
 * @see https://developer.chrome.com/apps/tags/webview#property-request
 */
WebView.prototype.request;

/**
 * @constructor
 * @see https://developer.chrome.com/apps/tags/webview#type-ContextMenus
 */
function ContextMenus() {}
ContextMenus.prototype.onShow;
ContextMenus.prototype.onShow.addListener = function() {};

/**
 * @type {ContextMenus}
 * @see https://developer.chrome.com/apps/tags/webview#property-contextMenus
 */
WebView.prototype.contextMenus;

/**
 * @typedef{{
 *   code: ?string,
 *   files: ?Array<string>
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-InjectionItems
 */
var InjectionItems;

/**
 * Details of the content script to inject.
 * @typedef{{
 *   name: string,
 *   matches: Array<string>,
 *   exclude_matches: ?Array<string>,
 *   match_about_blank: ?boolean,
 *   css: ?InjectionItems,
 *   js: ?InjectionItems,
 *   run_at: ?string,
 *   all_frames: ?boolean,
 *   include_globs: ?Array<string>,
 *   exclude_globs: ?Array<string>,
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#type-ContentScriptDetails
 */
var ContentScriptDetails;

/**
 * @param {Array<ContentScriptDetails>} contentScriptList
 * @see https://developer.chrome.com/apps/tags/webview#method-addContentScripts
 */
WebView.prototype.addContentScripts = function(contentScriptList) {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-back
 */
WebView.prototype.back = function() {};

/**
 * @return {boolean}
 * @see https://developer.chrome.com/apps/tags/webview#method-canGoBack
 */
WebView.prototype.canGoBack = function() {};

/**
 * @return {boolean}
 * @see https://developer.chrome.com/apps/tags/webview#method-canGoBack
 */
WebView.prototype.canGoForward = function() {};

/**
 * @param {ClearDataOptions} options
 * @param {ClearDataTypeSet} types
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-clearData
 */
WebView.prototype.clearData = function(options, types, opt_callback) {};

/**
 * @param {InjectDetails} details
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-executeScript
 */
WebView.prototype.executeScript = function(details, opt_callback) {};

/**
 * @param {string} searchText
 * @param {FindOptions=} opt_options
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-find
 */
WebView.prototype.find = function(searchText, opt_options, opt_callback) {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-forward
 */
WebView.prototype.forward = function() {};

/**
 * @return {number}
 * @see https://developer.chrome.com/apps/tags/webview#method-getProcessId
 */
WebView.prototype.getProcessId = function() {};

/**
 * @return {string}
 * @see https://developer.chrome.com/apps/tags/webview#method-getUserAgent
 */
WebView.prototype.getUserAgent = function() {};

/**
 * @param {Function} callback
 * @see https://developer.chrome.com/apps/tags/webview#method-getZoom
 */
WebView.prototype.getZoom = function(callback) {};

/**
 * @param {number} relativeIndex
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-go
 */
WebView.prototype.go = function(relativeIndex, opt_callback) {};

/**
 * @param {InjectDetails} details
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-insertCSS
 */
WebView.prototype.insertCSS = function(details, opt_callback) {};

/**
 * @return {boolean}
 * @see https://developer.chrome.com/apps/tags/webview#method-isUserAgentOverridden
 */
WebView.prototype.isUserAgentOverridden = function() {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-print
 */
WebView.prototype.print = function() {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-reload
 */
WebView.prototype.reload = function() {};

/**
 * @param {Array<string>} contentScriptList
 * @see https://developer.chrome.com/apps/tags/webview#method-removeContentScripts
 */
WebView.prototype.removeContentScripts = function(contentScriptList) {};

/**
 * @param {string} userAgent
 * @see https://developer.chrome.com/apps/tags/webview#method-setUserAgentOverride
 */
WebView.prototype.setUserAgentOverride = function(userAgent) {};

/**
 * @param {number} zoomFactor
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-setZoom
 */
WebView.prototype.setZoom = function(zoomFactor, opt_callback) {};

/**
 * @param {string} zoomMode Allowed values: "per-origin", "per-view", "disabled"
 * @see https://developer.chrome.com/apps/tags/webview#type-ZoomMode
 * @param {Function=} opt_callback
 * @see https://developer.chrome.com/apps/tags/webview#method-setZoomMode
 */
WebView.prototype.setZoomMode = function(zoomMode, opt_callback) {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-stop
 */
WebView.prototype.stop = function() {};

/**
 * @param {?=} opt_action
 */
WebView.prototype.stopFinding = function(opt_action) {};

/**
 * @param {string} dataUrl
 * @param {string} baseUrl
 * @param {string=} opt_virtualUrl
 * @see https://developer.chrome.com/apps/tags/webview#method-stopFinding
 */
WebView.prototype.loadDataWithBaseUrl =
    function(dataUrl, baseUrl, opt_virtualUrl) {};

/**
 * @see https://developer.chrome.com/apps/tags/webview#method-terminate
 */
WebView.prototype.terminate = function() {};

/**
 * @constructor
 * @extends {Event}
 */
function NewWindowEvent() {}

/** @type {NewWindow} */
NewWindowEvent.prototype.window;

/** @type {string} */
NewWindowEvent.prototype.targetUrl;

/** @type {number} */
NewWindowEvent.prototype.initialWidth;

/** @type {number} */
NewWindowEvent.prototype.initialHeight;

/** @type {string} */
NewWindowEvent.prototype.name;


/**
 * @typedef {{
 *   url: string,
 *   isTopLevel: boolean,
 *   code: number,
 *   reason: string
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#event-loadabort
 */
var WebviewLoadAbortEventData;


/** @type {!ChromeEvent} */
WebView.prototype.close;

/** @type {!ChromeEvent} */
WebView.prototype.consolemessage;

/** @type {!ChromeEvent} */
WebView.prototype.contentload;

/** @type {!ChromeEvent} */
WebView.prototype.dialog;

/** @type {!ChromeEvent} */
WebView.prototype.exit;

/** @type {!ChromeEvent} */
WebView.prototype.findupdate;

/** @type {!ChromeBaseEvent<WebviewLoadAbortEventData>} */
WebView.prototype.loadabort;

/** @type {!ChromeEvent} */
WebView.prototype.loadcommit;

/** @type {!ChromeEvent} */
WebView.prototype.loadredirect;

/** @type {!ChromeEvent} */
WebView.prototype.loadstart;

/** @type {!ChromeEvent} */
WebView.prototype.loadstop;

/**
 * "newwindow" event object properties.
 * @typedef{{
 *   window: NewWindow,
 *   targetUrl: string,
 *   initialWidth: number,
 *   initialHeight: number,
 *   name: string,
 *   windowOpenDisposition: string
 * }}
 * @see https://developer.chrome.com/apps/tags/webview#event-newwindow
 */
var NewWindowProperties;

/** @type {!ChromeEvent} */
WebView.prototype.newwindow;

/** @type {!ChromeEvent} */
WebView.prototype.permissionrequest;

/** @type {!ChromeEvent} */
WebView.prototype.responsive;

/** @type {!ChromeEvent} */
WebView.prototype.sizechanged;

/** @type {!ChromeEvent} */
WebView.prototype.unresponsive;

/** @type {!ChromeEvent} */
WebView.prototype.zoomchange;

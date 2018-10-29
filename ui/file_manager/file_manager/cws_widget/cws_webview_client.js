// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {WebView} webView Web View tag.
 * @param {number} width Width of the CWS widget.
 * @param {number} height Height of the CWS widget.
 * @param {string} url Share Url for an entry.
 * @param {string} target Target (scheme + host + port) of the widget.
 * @param {Object<*>} options Options to be sent to the dialog host.
 * @param {!CWSWidgetContainerPlatformDelegate} delegate Delegate for accessing
 *     Chrome platform APIs.
 * @constructor
 * @extends {cr.EventTarget}
 */
function CWSContainerClient(
    webView, width, height, url, target, options, delegate) {
  /** @private {!CWSWidgetContainerPlatformDelegate} */
  this.delegate_ = delegate;
  this.webView_ = webView;
  this.width_ = width;
  this.height_ = height;
  this.url_ = url;
  this.target_ = target;
  this.options_ = options;

  this.loaded_ = false;
  this.loading_ = false;

  this.onMessageBound_ = this.onMessage_.bind(this);
  this.onLoadStopBound_ = this.onLoadStop_.bind(this);
  this.onLoadAbortBound_ = this.onLoadAbort_.bind(this);
}

CWSContainerClient.prototype = {
  __proto__: cr.EventTarget.prototype
};

/**
 * Events CWSContainerClient fires
 *
 * @enum {string}
 * @const
 */
CWSContainerClient.Events = {
  LOADED: 'CWSContainerClient.Events.LOADED',
  LOAD_FAILED: 'CWSContainerClient.Events.LOAD_FAILED',
  REQUEST_INSTALL: 'CWSContainerClient.Events.REQUEST_INSTALL',
  INSTALL_DONE: 'CWSContainerClient.Events.INSTALL_DONE'
};
Object.freeze(CWSContainerClient.Events);

/**
 * Handles messages from the widget
 * @param {Event} event Message event.
 * @private
 */
CWSContainerClient.prototype.onMessage_ = function(event) {
  if (event.origin != this.target_)
    return;

  var data = event.data;
  switch (data['message']) {
    case 'widget_loaded':
      this.onWidgetLoaded_();
      break;
    case 'widget_load_failed':
      this.onWidgetLoadFailed_();
      break;
    case 'before_install':
      this.sendInstallRequest_(data['item_id']);
      break;
    case 'after_install':
      this.sendInstallDone_(data['item_id']);
      break;
    default:
      console.error('Unexpected message: ' + data['message'], data);
  }
};

/**
 * Called when receiving 'loadstop' event from the <webview>.
 * @param {Event} event Message event.
 * @private
 */
CWSContainerClient.prototype.onLoadStop_ = function(event) {
  if (this.url_ == this.webView_.src && !this.loaded_) {
    this.loaded_ = true;
    this.postInitializeMessage_();
  }
};

/**
 * Called when the widget is loaded successfully.
 * @private
 */
CWSContainerClient.prototype.onWidgetLoaded_ = function() {
  cr.dispatchSimpleEvent(this, CWSContainerClient.Events.LOADED);
};

/**
 * Called when the widget is failed to load.
 * @private
 */
CWSContainerClient.prototype.onWidgetLoadFailed_ = function() {
  this.sendWidgetLoadFailed_();
};

/**
 * Called when receiving the 'loadabort' event from <webview>.
 * @param {Event} event Message event.
 * @private
 */
CWSContainerClient.prototype.onLoadAbort_ = function(event) {
  this.sendWidgetLoadFailed_();
};

/**
 * Called when the installation is completed from the suggest-app dialog.
 *
 * @param {boolean} result True if the installation is success, false if failed.
 * @param {string} itemId Item id to be installed.
 */
CWSContainerClient.prototype.onInstallCompleted = function(result, itemId) {
  if (result)
    this.postInstallSuccessMessage_(itemId);
  else
    this.postInstallFailureMessage_(itemId);
};

/**
 * Send the fail message to the suggest-app dialog.
 * @private
 */
CWSContainerClient.prototype.sendWidgetLoadFailed_ = function() {
  cr.dispatchSimpleEvent(this, CWSContainerClient.Events.LOAD_FAILED);
};

/**
 * Send the install request to the suggest-app dialog.
 *
 * @param {string} itemId Item id to be installed.
 * @private
 */
CWSContainerClient.prototype.sendInstallRequest_ = function(itemId) {
  var event = new Event(CWSContainerClient.Events.REQUEST_INSTALL);
  event.itemId = itemId;
  this.dispatchEvent(event);
};

/**
 * Notifies the suggest-app dialog that the item installation is completed.
 *
 * @param {string} itemId The installed item ID.
 * @private
 */
CWSContainerClient.prototype.sendInstallDone_ = function(itemId) {
  var event = new Event(CWSContainerClient.Events.INSTALL_DONE);
  event.itemId = itemId;
  this.dispatchEvent(event);
};

/**
 * Send the 'install_failure' message to the widget.
 *
 * @param {string} itemId Item id to be installed.
 * @private
 */
CWSContainerClient.prototype.postInstallFailureMessage_ = function(itemId) {
  var message = {message: 'install_failure', item_id: itemId, v: 1};

  this.postMessage_(message);
};

/**
 * Send the 'install_success' message to the widget.
 *
 * @param {string} itemId Item id to be installed.
 * @private
 */
CWSContainerClient.prototype.postInstallSuccessMessage_ = function(itemId) {
  var message = {message: 'install_success', item_id: itemId, v: 1};

  this.postMessage_(message);
};

/**
 * Send the 'initialize' message to the widget.
 * @private
 */
CWSContainerClient.prototype.postInitializeMessage_ = function() {
  new Promise(function(fulfill, reject) {
    this.delegate_.getInstalledItems(
        /**
         * @param {?Array<!string>} items Installed items.
         *     Null on error.
         */
        function(items) {
          if (!items) {
            reject('Failed to retrive installed items.');
            return;
          }
          fulfill(items);
        });
  }.bind(this))
      .then((/**
              * @param {!Array<string>} preinstalledExtensionIDs
              */
             function(preinstalledExtensionIDs) {
               var message = {
                 message: 'initialize',
                 hl: this.delegate_.strings.UI_LOCALE,
                 width: this.width_,
                 height: this.height_,
                 preinstalled_items: preinstalledExtensionIDs,
                 v: 1
               };

               if (this.options_) {
                 Object.keys(this.options_).forEach(function(key) {
                   message[key] = this.options_[key];
                 }.bind(this));
               }

               this.postMessage_(message);
             }).bind(this));
};

/**
 * Send a message to the widget. This method shouldn't be called directly,
 * should from more specified posting function (eg. postXyzMessage_()).
 *
 * @param {Object} message Message object to be posted.
 * @private
 */
CWSContainerClient.prototype.postMessage_ = function(message) {
  if (!this.webView_.contentWindow)
    return;

  this.webView_.contentWindow.postMessage(message, this.target_);
};

/**
 * Loads the page to <webview>. Can be called only once.
 */
CWSContainerClient.prototype.load = function() {
  if (this.loading_ || this.loaded_)
    throw new Error('Already loaded.');
  this.loading_ = true;
  this.loaded_ = false;

  window.addEventListener('message', this.onMessageBound_);
  this.webView_.addEventListener('loadstop', this.onLoadStopBound_);
  this.webView_.addEventListener('loadabort', this.onLoadAbortBound_);
  this.webView_.setAttribute('src', this.url_);
};

/**
 * Aborts loading of the embedded dialog and performs cleanup.
 */
CWSContainerClient.prototype.abort = function() {
  window.removeEventListener('message', this.onMessageBound_);
  this.webView_.removeEventListener('loadstop', this.onLoadStopBound_);
  this.webView_.removeEventListener('loadabort', this.onLoadAbortBound_);
  this.webView_.stop();
};

/**
 * Cleans the dialog by removing all handlers.
 */
CWSContainerClient.prototype.dispose = function() {
  this.abort();
};

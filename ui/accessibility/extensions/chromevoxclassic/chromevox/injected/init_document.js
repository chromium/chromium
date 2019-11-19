// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Initializes the injected content script on the document.
 *
 * NOTE(deboer): This file will automatically initialize ChromeVox.  If you can
 * control when ChromeVox starts, consider using cvox.InitGlobals instead.
 *
 */

goog.provide('cvox.ChromeVoxInit');

goog.require('cvox.ChromeVox');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.HostFactory');
goog.require('cvox.InitGlobals');

/**
 * The time to pause before trying again to initialize, in ms. This
 * number starts small and keeps growing so that we don't waste CPU
 * time on a page that takes a long time to load.
 * @type {number}
 * @private
 */
cvox.ChromeVox.initTimeout_ = 100;


/**
 * @type {number}
 * @private
 */
cvox.ChromeVox.initTimer_ = 0;


/**
 * Maximum retry timeout for initialization.  Note that exponential backoff
 * used, so the actual time before giving up is about twice this number.
 * @const {number}
 * @private
 */
cvox.ChromeVox.MAX_INIT_TIMEOUT_ = 30000;


/**
 * Flag indicating if ChromeVox Classic is enabled based on the Next
 * background page  which sends the state at page load.
 * @type {boolean|undefined}
 * @private
 */
cvox.ChromeVox.isClassicEnabled_ = undefined;


/**
 * Call the init function later, safely.
 * @param {string} reason A developer-readable string to log to the console
 *    explaining why we're trying again.
 * @private
 */
cvox.ChromeVox.recallInit_ = function(reason) {
  if (cvox.ChromeVox.initTimeout_ > cvox.ChromeVox.MAX_INIT_TIMEOUT_) {
    window.console.log(reason +
        ' Taking too long - giving up.');
    return;
  }
  window.console.log(reason +
                     ' Will try again in ' +
                     cvox.ChromeVox.initTimeout_ + 'ms');
  cvox.ChromeVox.initTimer_ = window.setTimeout(
      cvox.ChromeVox.initDocument,
      cvox.ChromeVox.initTimeout_);
  cvox.ChromeVox.initTimeout_ *= 2;
};


/**
 * Initializes cvox.ChromeVox when the document is ready.
 */
cvox.ChromeVox.initDocument = function() {
  // Don't start the content script on the ChromeVox background page.
  if (/^chrome-extension:\/\/.*background\.html$/.test(window.location.href)) {
    return;
  }

  // Look for ChromeVox-specific meta attributes.
  var disableContentScript = false;
  if (document.head) {
    document.head.querySelectorAll('meta[name="chromevox"]').forEach(
      function(meta) {
        var contentScript = meta.getAttribute('content-script');
        if (contentScript && contentScript.toLowerCase() == 'no') {
          disableContentScript = true;
        }
      });
  }
  if (disableContentScript) {
    var url = location.href;
    url = url.substring(0, url.indexOf('#')) || url;
    cvox.ExtensionBridge.send({
      target: 'next',
      action: 'enableCompatForUrl',
      url: url
    });
    return;
  }

  cvox.ExtensionBridge.send({
    target: 'next',
    action: 'getIsClassicEnabled',
    url: location.href
  });

  cvox.ChromeVox.initTimer_ = 0;
  var reinitReason;
  if (!document.body) {
    reinitReason = 'ChromeVox not starting on unloaded page';
  }
  if (cvox.ChromeVox.isClassicEnabled_ === undefined) {
    reinitReason = 'ChromeVox waiting for background page';
  }
  if (reinitReason) {
    cvox.ChromeVox.recallInit_(reinitReason + ': ' +
        document.location.href + '.');
    return;
  }

  if (!cvox.ChromeVox.isClassicEnabled_) {
    return;
  }

  // Setup globals
  cvox.ChromeVox.host = cvox.HostFactory.getHost();

  if (!cvox.ChromeVox.host.ttsLoaded()) {
    cvox.ChromeVox.recallInit_('ChromeVox not starting; waiting for TTS. ' +
                               document.location.href + '.');
    return;
  }

  window.console.log('Starting ChromeVox.');

  cvox.InitGlobals.initGlobals();

  // Add a global function to disable this instance of ChromeVox.
  // There is a scenario where two copies of the content script can get
  // loaded into the same tab on browser startup - one automatically
  // and one because the background page injects the content script into
  // every tab on startup. This allows the background page to first deactivate
  // any existing copy of the content script (if any) before loading it again,
  // otherwise there can be duplicate event listeners.
  window.disableChromeVox = function() {
    cvox.ChromeVox.host.killChromeVox();
  };
};


/**
 * Reinitialize ChromeVox, if the extension is disabled and then enabled
 * again, but our injected page script has remained.
 */
cvox.ChromeVox.reinit = function() {
  cvox.ChromeVox.host.reinit();
  cvox.ChromeVox.initDocument();
};

if (!COMPILED) {
  // NOTE(deboer): This is called when this script is loaded, automatically
  // starting ChromeVox. If this isn't the uncompiled script, it will be
  // called in loader.js.
  cvox.ChromeVox.initDocument();
}

cvox.ExtensionBridge.addMessageListener(function(msg, port) {
  if (msg['target'] == 'next') {
    cvox.ChromeVox.isClassicEnabled_ = msg['isClassicEnabled'];
  }
});

cvox.ExtensionBridge.addDisconnectListener(function() {
  if (cvox.ChromeVox.initTimer_ > 0) {
    window.clearTimeout(cvox.ChromeVox.initTimer_);
    cvox.ChromeVox.initTimer_ = 0;
  }
});

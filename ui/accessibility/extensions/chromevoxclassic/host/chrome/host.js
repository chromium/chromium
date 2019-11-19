// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Chrome-specific implementation of methods that differ
 * depending on the host platform.
 *
 */

goog.provide('cvox.ChromeHost');

goog.require('cvox.AbstractHost');
goog.require('cvox.ApiImplementation');
goog.require('cvox.BrailleOverlayWidget');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxEventWatcher');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.HostFactory');
goog.require('cvox.InitialSpeech');
goog.require('cvox.PdfProcessor');
goog.require('cvox.SearchLoader');
goog.require('cvox.TraverseMath');

/**
 * @constructor
 * @extends {cvox.AbstractHost}
 */
cvox.ChromeHost = function() {
  goog.base(this);

  /** @private {boolean} */
  this.gotPrefsAtLeastOnce_ = false;
};
goog.inherits(cvox.ChromeHost, cvox.AbstractHost);


/** @override */
cvox.ChromeHost.prototype.init = function() {
  // TODO(deboer): This pattern is relatively painful since it
  // must be duplicated in all host.js files. It also causes odd
  // dependencies.
  // TODO (stoarca): Not using goog.bind because for some reason it gets
  // compiled to native code and not possible to debug.
  var self = this;
  var listener = function(message) {
    if (message['history']) {
      cvox.ChromeVox.visitedUrls = message['history'];
    }

    if (message['keyBindings']) {
      cvox.ChromeVoxKbHandler.loadKeyToFunctionsTable(message['keyBindings']);
    }
    if (message['prefs']) {
      var prefs = message['prefs'];
      cvox.ChromeVoxEditableTextBase.useIBeamCursor =
          (prefs['useIBeamCursor'] == 'true');
      cvox.ChromeVoxEditableTextBase.eventTypingEcho = true;
      cvox.ChromeVoxEventWatcher.focusFollowsMouse =
          (prefs['focusFollowsMouse'] == 'true');

      cvox.ChromeVox.version = prefs['version'];

      cvox.ChromeVox.typingEcho =
          /** @type {number} */ (JSON.parse(prefs['typingEcho']));

      if (prefs['position']) {
        cvox.ChromeVox.position =
            /** @type {Object<{x:number, y:number}>} */ (
                JSON.parse(prefs['position']));
      }

      if (prefs['granularity'] != 'undefined') {
        cvox.ChromeVox.navigationManager.setGranularity(
            /** @type {number} */ (JSON.parse(prefs['granularity'])));
      }

      self.activateOrDeactivateChromeVox(prefs['active'] == 'true');
      self.activateOrDeactivateStickyMode(prefs['sticky'] == 'true');
      if (!self.gotPrefsAtLeastOnce_) {
        cvox.InitialSpeech.speak();
      }
      self.gotPrefsAtLeastOnce_ = true;

      if (prefs['useVerboseMode'] == 'false') {
        cvox.ChromeVox.verbosity = cvox.VERBOSITY_BRIEF;
      } else {
        cvox.ChromeVox.verbosity = cvox.VERBOSITY_VERBOSE;
      }
      if (prefs['cvoxKey']) {
        cvox.ChromeVox.modKeyStr = prefs['cvoxKey'];
      }

      var apiPrefsChanged =
          (!cvox.ApiImplementation.siteSpecificScriptLoader ||
           !cvox.ApiImplementation.siteSpecificScriptBase);
      cvox.ApiImplementation.siteSpecificScriptLoader =
          'https://ssl.gstatic.com/accessibility/javascript/ext/loader.js';
      cvox.ApiImplementation.siteSpecificScriptBase =
          'https://ssl.gstatic.com/accessibility/javascript/ext/';
      if (apiPrefsChanged) {
        var searchInit = prefs['siteSpecificEnhancements'] === 'true' ?
            cvox.SearchLoader.init :
            undefined;
        cvox.ApiImplementation.init(searchInit);
      }
      cvox.BrailleOverlayWidget.getInstance().setActive(
          prefs['brailleCaptions'] == 'true');
    }
  };
  cvox.ExtensionBridge.addMessageListener(listener);

  cvox.ExtensionBridge.addMessageListener(function(msg, port) {
    if (msg['message'] == 'DOMAINS_STYLES') {
      cvox.TraverseMath.getInstance().addDomainsAndStyles(
          msg['domains'], msg['styles']);
    }
  });

  cvox.ExtensionBridge.addMessageListener(function(msg, port) {
    var message = msg['message'];
    var cmd = msg['command'];
    if (message == 'USER_COMMAND') {
      if (cmd != 'toggleChromeVox' && !cvox.ChromeVox.documentHasFocus()) {
        return;
      }
      cvox.ChromeVoxUserCommands.commands[cmd](msg);
    } else if (message == 'SYSTEM_COMMAND') {
      if (cmd == 'killChromeVox') {
        this.killChromeVox();
      }
    }
  }.bind(this));

  cvox.ExtensionBridge.send({'target': 'Prefs', 'action': 'getPrefs'});

  cvox.ExtensionBridge.send({'target': 'Data', 'action': 'getHistory'});
};


/** @override */
cvox.ChromeHost.prototype.reinit = function() {
  cvox.ExtensionBridge.init();
};


/** @override */
cvox.ChromeHost.prototype.onPageLoad = function() {
  cvox.PdfProcessor.processEmbeddedPdfs();

  cvox.ExtensionBridge.addDisconnectListener(goog.bind(function() {
    cvox.ChromeVox.isActive = false;
    cvox.ChromeVoxEventWatcher.cleanup(window);
    // TODO(stoarca): Huh?? Why are we resetting during disconnect?
    // This is not appropriate behavior!
    cvox.ChromeVox.navigationManager.reset();
  }, this));
};


/** @override */
cvox.ChromeHost.prototype.sendToBackgroundPage = function(message) {
  cvox.ExtensionBridge.send(message);
};


/** @override */
cvox.ChromeHost.prototype.getApiSrc = function() {
  return this.getFileSrc('chromevox/injected/api.js');
};


/** @override */
cvox.ChromeHost.prototype.getFileSrc = function(file) {
  return window.chrome.extension.getURL(file);
};


/** @override */
cvox.ChromeHost.prototype.killChromeVox = function() {
  goog.base(this, 'killChromeVox');
  cvox.ExtensionBridge.removeMessageListeners();
};


/**
 * Activates or deactivates Sticky Mode.
 * @param {boolean} sticky Whether sticky mode should be active.
 */
cvox.ChromeHost.prototype.activateOrDeactivateStickyMode = function(sticky) {
  cvox.ChromeVox.isStickyPrefOn = sticky;
};

/**
 * The host constructor for Chrome.
 */
cvox.HostFactory.hostConstructor = cvox.ChromeHost;

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Abstract interface to methods that differ depending on the
 * host platform.
 *
 */

goog.provide('cvox.AbstractHost');

goog.require('cvox.ChromeVoxEventWatcher');

/**
 * @constructor
 */
cvox.AbstractHost = function() {
};


/**
 * @enum {number}
 */
cvox.AbstractHost.State = {
  ACTIVE: 0,
  INACTIVE: 1,
  KILLED: 2
};


/**
 * Do all host-platform-specific initialization.
 */
cvox.AbstractHost.prototype.init = function() {
};


/**
 * Used to reinitialize ChromeVox if initialization fails.
 */
cvox.AbstractHost.prototype.reinit = function() {
};


/**
 * Executed on page load.
 */
cvox.AbstractHost.prototype.onPageLoad = function() {
};


/**
 * Sends a message to the background page (if it exists) for this host.
 * @param {Object} message The message to pass to the background page.
 */
cvox.AbstractHost.prototype.sendToBackgroundPage = function(message) {
};


/**
 * Returns the absolute URL to the API source.
 * @return {string} The URL.
 */
cvox.AbstractHost.prototype.getApiSrc = function() {
  return '';
};


/**
 * Return the absolute URL to the given file.
 * @param {string} path The URL suffix.
 * @return {string} The full URL.
 */
cvox.AbstractHost.prototype.getFileSrc = function(path) {
  return '';
};


/**
 * @return {boolean} True if the host has a Tts callback.
 */
cvox.AbstractHost.prototype.hasTtsCallback = function() {
  return true;
};


/**
 * @return {boolean} True if the TTS has been loaded.
 */
cvox.AbstractHost.prototype.ttsLoaded = function() {
  return true;
};


/**
 * @return {boolean} True if the ChromeVox is supposed to intercept and handle
 * mouse clicks for the platform, instead of just letting the clicks fall
 * through.
 *
 * Note: This behavior is only needed for Android because of the way touch
 * exploration and double-tap to click is implemented by the platform.
 */
cvox.AbstractHost.prototype.mustRedispatchClickEvent = function() {
  return false;
};

/**
 * Activate or deactivate ChromeVox on this host.
 * @param {boolean} active The desired state; true for active, false for
 * inactive.
 */
cvox.AbstractHost.prototype.activateOrDeactivateChromeVox = function(active) {
  this.onStateChanged_(active ? cvox.AbstractHost.State.ACTIVE :
      cvox.AbstractHost.State.INACTIVE);
};


/**
 * Kills ChromeVox on this host.
 */
cvox.AbstractHost.prototype.killChromeVox = function() {
  this.onStateChanged_(cvox.AbstractHost.State.KILLED);
};


/**
 * Helper managing the three states of ChromeVox --
 * active: all event listeners registered
 * inactive: only key down listener registered
 * killed: no listeners registered
 * @param {cvox.AbstractHost.State} state The new state.
 * @private
 */
cvox.AbstractHost.prototype.onStateChanged_ = function(state) {
  var active = state == cvox.AbstractHost.State.ACTIVE;
  if (active == cvox.ChromeVox.isActive) {
    return;
  }
  cvox.ChromeVoxEventWatcher.cleanup(window);
  switch (state) {
    case cvox.AbstractHost.State.ACTIVE:
      cvox.ChromeVox.isActive = true;
      cvox.ChromeVox.navigationManager.showOrHideIndicator(true);
      cvox.ChromeVoxEventWatcher.init(window);
      if (document.activeElement) {
        var speakNodeAlso = cvox.ChromeVox.documentHasFocus();
        cvox.ApiImplementation.syncToNode(
            document.activeElement, speakNodeAlso);
      } else {
        cvox.ChromeVox.navigationManager.updateIndicator();
      }
      break;
    case cvox.AbstractHost.State.INACTIVE:
      cvox.ChromeVox.isActive = false;
      cvox.ChromeVox.navigationManager.showOrHideIndicator(false);
      // If ChromeVox is inactive, the event watcher will only listen for key
      // down events.
      cvox.ChromeVoxEventWatcher.init(window);
      break;
    case cvox.AbstractHost.State.KILLED:
      cvox.ChromeVox.isActive = false;
      cvox.ChromeVox.navigationManager.showOrHideIndicator(false);
      break;
  }
};

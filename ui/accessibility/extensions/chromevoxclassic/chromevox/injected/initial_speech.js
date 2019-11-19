// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the initial speech call.
 */

goog.provide('cvox.InitialSpeech');

goog.require('cvox.AbstractTts');
goog.require('cvox.BrailleOverlayWidget');
goog.require('cvox.ChromeVox');
goog.require('cvox.CursorSelection');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.LiveRegions');

// INJECTED_AFTER_LOAD is set true by ChromeVox itself or ChromeOS when this
// script is injected after page load (i.e. when manually enabling ChromeVox).
if (!window['INJECTED_AFTER_LOAD'])
  window['INJECTED_AFTER_LOAD'] = false;


/**
 * Initial speech when the page loads. This may happen only after we get
 * prefs back, so we can make sure ChromeVox is active.
 */
cvox.InitialSpeech.speak = function() {
  // Don't speak page title and other information if this script is not injected
  // at the time of page load. This global is set by Chrome OS.
  var disableSpeak = window['INJECTED_AFTER_LOAD'];

  if (!cvox.ChromeVox.isActive || document.webkitHidden) {
    disableSpeak = true;
  }

  // If we're the top-level frame, speak the title of the page +
  // the active element if it is a user control.
  if (window.top == window) {
    var title = document.title;

    // Allow the web author to disable reading the page title on load
    // by adding aria-hidden=true to the <title> element.
    var titleElement = document.querySelector('head > title');
    if (titleElement && titleElement.getAttribute('aria-hidden') == 'true') {
      title = null;
    }

    if (title && !disableSpeak) {
      cvox.ChromeVox.tts.speak(
          title, cvox.QueueMode.FLUSH);
    }
    cvox.BrailleOverlayWidget.getInstance().init();
  }

  // Initialize live regions and speak alerts.
  cvox.LiveRegions.init(
      new Date(), cvox.QueueMode.QUEUE, disableSpeak);

  // If our activeElement is on body, try to sync to the first element. This
  // actually happens inside of NavigationManager.reset, which doesn't get
  // called until AbstractHost.onPageLoad, but we need to speak and braille the
  // initial node here.
  if (cvox.ChromeVox.documentHasFocus() &&
      document.activeElement == document.body) {
    cvox.ChromeVox.navigationManager.syncToBeginning();
  }

  // If we had a previous position recorded, update to it.
  if (cvox.ChromeVox.position[document.location.href]) {
    var pos = cvox.ChromeVox.position[document.location.href];
    cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(
        document.elementFromPoint(pos.x, pos.y));
  }

  // If this iframe has focus, speak and braille the current focused element.
  if (cvox.ChromeVox.documentHasFocus()) {
    if (!disableSpeak) {
      cvox.ChromeVoxEventSuspender.withSuspendedEvents(function() {
        cvox.ChromeVox.navigationManager.finishNavCommand(
            '', true, cvox.QueueMode.QUEUE);
      })();
    }
  }
};

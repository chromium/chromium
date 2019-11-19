// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Initializes the injected content script.
 *
 */

goog.provide('cvox.InitGlobals');

goog.require('cvox.ApiImplementation');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxEventWatcher');
goog.require('cvox.CompositeTts');
goog.require('cvox.ConsoleTts');
goog.require('cvox.HostFactory');
goog.require('cvox.NavigationManager');
goog.require('cvox.Serializer');



/**
 * @constructor
 */
cvox.InitGlobals = function() { };


/**
 * Initializes cvox.ChromeVox.
 */
cvox.InitGlobals.initGlobals = function() {
  if (!cvox.ChromeVox.host) {
    cvox.ChromeVox.host = cvox.HostFactory.getHost();
  }

  cvox.ChromeVox.tts = new cvox.CompositeTts()
      .add(cvox.HostFactory.getTts())
      .add(cvox.History.getInstance())
      .add(cvox.ConsoleTts.getInstance());

  if (!cvox.ChromeVox.braille) {
    cvox.ChromeVox.braille = cvox.HostFactory.getBraille();
  }
  cvox.ChromeVox.mathJax = cvox.HostFactory.getMathJax();

  cvox.ChromeVox.earcons = cvox.HostFactory.getEarcons();
  cvox.ChromeVox.isActive = true;
  cvox.ChromeVox.navigationManager = new cvox.NavigationManager();
  cvox.ChromeVox.navigationManager.updateIndicator();
  cvox.ChromeVox.syncToNode = cvox.ApiImplementation.syncToNode;
  cvox.ChromeVox.speakNode = cvox.ApiImplementation.speakNode;

  cvox.ChromeVox.serializer = new cvox.Serializer();

  // Do platform specific initialization here.
  cvox.ChromeVox.host.init();

  // Start the event watchers
  cvox.ChromeVoxEventWatcher.init(window);

  // Provide a way for modules that can't depend on cvox.ChromeVoxUserCommands
  // to execute commands.
  cvox.ChromeVox.executeUserCommand = function(commandName) {
    cvox.ChromeVoxUserCommands.commands[commandName]();
  };

  cvox.ChromeVox.host.onPageLoad();
};

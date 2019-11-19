// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge that sends earcon messages from content scripts or
 * other pages to the main background page.
 *
 */


goog.provide('cvox.ChromeEarcons');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.HostFactory');


/**
 * @constructor
 * @extends {cvox.AbstractEarcons}
 */
cvox.ChromeEarcons = function() {
  goog.base(this);
};
goog.inherits(cvox.ChromeEarcons, cvox.AbstractEarcons);


/**
 * @override
 */
cvox.ChromeEarcons.prototype.playEarcon = function(earcon) {
  goog.base(this, 'playEarcon', earcon);
  if (!cvox.AbstractEarcons.enabled) {
    return;
  }

  cvox.ExtensionBridge.send({
                              'target': 'EARCON',
                              'action': 'play',
                              'earcon': earcon});
};


/**
 * @override
 */
cvox.ChromeEarcons.prototype.toggle = function() {
  goog.base(this, 'toggle');
  cvox.ChromeVox.host.sendToBackgroundPage({
    'target': 'Prefs',
    'action': 'setPref',
    'pref': 'earcons',
    'value': cvox.AbstractEarcons.enabled
  });
  if (!cvox.AbstractEarcons.enabled) {
    cvox.ChromeVox.host.sendToBackgroundPage({
      'target': 'Prefs',
      'action': 'setPref',
      'pref': 'useVerboseMode',
      'value': true
    });
  }
  return cvox.AbstractEarcons.enabled;
};


/**
 * @override
 */
cvox.HostFactory.earconsConstructor = cvox.ChromeEarcons;

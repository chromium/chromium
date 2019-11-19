// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses the HTML5 Audio element to play back
 * auditory cues.
 *
 */


goog.provide('cvox.ClassicEarcons');

goog.require('cvox.AbstractEarcons');


/**
 * @constructor
 * @extends {cvox.AbstractEarcons}
 */
cvox.ClassicEarcons = function() {
  goog.base(this);

  if (localStorage['earcons'] === 'false') {
    cvox.AbstractEarcons.enabled = false;
  }

  this.audioMap = new Object();
};
goog.inherits(cvox.ClassicEarcons, cvox.AbstractEarcons);


/**
 * @return {string} The human-readable name of the earcon set.
 */
cvox.ClassicEarcons.prototype.getName = function() {
  return 'ChromeVox earcons';
};


/**
 * @return {string} The base URL for loading earcons.
 */
cvox.ClassicEarcons.prototype.getBaseUrl = function() {
  return cvox.ClassicEarcons.BASE_URL;
};


/**
 * @override
 */
cvox.ClassicEarcons.prototype.playEarcon = function(earcon) {
  goog.base(this, 'playEarcon', earcon);
  if (!cvox.AbstractEarcons.enabled) {
    return;
  }
  console.log('Earcon ' + earcon);

  this.currentAudio = this.audioMap[earcon];
  if (!this.currentAudio) {
    this.currentAudio = new Audio(chrome.extension.getURL(this.getBaseUrl() +
        earcon + '.ogg'));
    this.audioMap[earcon] = this.currentAudio;
  }
  try {
    this.currentAudio.currentTime = 0;
  } catch (e) {
  }
  if (this.currentAudio.paused) {
    this.currentAudio.volume = 0.7;
    this.currentAudio.play();
  }
};


/**
 * @override
 */
cvox.ClassicEarcons.prototype.cancelEarcon = function(earcon) {
  // Do nothing, all of the earcons are short, and they stop automatically
  // when done.
};


/**
 * The base URL for  loading eracons.
 * @type {string}
 */
cvox.ClassicEarcons.BASE_URL = 'chromevox/background/earcons/';

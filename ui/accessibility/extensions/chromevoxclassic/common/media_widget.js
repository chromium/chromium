// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxHTMLMediaWidget');

/**
 * @fileoverview Gives the user spoken feedback as they interact with the HTML5
 * media widgets (<video> and <audio>) + makes the widget keyboard accessible.
 *
 */

/**
 * A class containing the information needed to speak
 * a media element to the user.
 *
 * @constructor
 * @param {Element} mediaElem The media widget element.
 * @param {cvox.TtsInterface} tts The TTS object from ChromeVox.
 */
cvox.ChromeVoxHTMLMediaWidget = function(mediaElem, tts){
  var self = this;
  this.mediaElem_ = mediaElem;
  this.mediaTts_ = tts;

  this.keyListener_ = function(evt) {
    self.eventHandler_(evt);
  }
  this.blurListener_ = function(evt) {
    self.shutdown();
  }

  this.mediaElem_.addEventListener('keydown', this.keyListener_, false);
  this.mediaElem_.addEventListener('keyup', this.keyListener_, false);
  this.mediaElem_.addEventListener('blur', this.blurListener_, false);
};

/**
 * Removes the key listeners for the media widget.
 */
cvox.ChromeVoxHTMLMediaWidget.prototype.shutdown = function() {
  this.mediaElem_.removeEventListener('blur', this.blurListener_, false);
  this.mediaElem_.removeEventListener('keydown', this.keyListener_, false);
  this.mediaElem_.removeEventListener('keyup', this.keyListener_, false);
};

cvox.ChromeVoxHTMLMediaWidget.prototype.jumpToTime_ = function(targetTime) {
  if (targetTime < 0) {
    targetTime = 0;
  }
  if (targetTime > this.mediaElem_.duration) {
    targetTime = this.mediaElem_.duration;
  }
  this.mediaElem_.currentTime = targetTime;
};

cvox.ChromeVoxHTMLMediaWidget.prototype.setVolume_ = function(targetVolume) {
  if (targetVolume < 0) {
    targetVolume = 0;
  }
  if (targetVolume > 1.0) {
    targetVolume = 1.0;
  }
  this.mediaElem_.volume = targetVolume;
};

/**
 * Adds basic keyboard handlers to the media widget.
 */
cvox.ChromeVoxHTMLMediaWidget.prototype.eventHandler_ = function(evt) {
  if (evt.type == 'keydown') {
    // Space/Enter for play/pause toggle.
    if ((evt.keyCode == 13) || (evt.keyCode == 32)) {
      if (this.mediaElem_.paused){
        this.mediaElem_.play();
      } else {
        this.mediaElem_.pause();
      }
    } else if (evt.keyCode == 39) { // Right - FF
      this.jumpToTime_(
          this.mediaElem_.currentTime + (this.mediaElem_.duration/10));
    } else if (evt.keyCode == 37) { // Left - REW
      this.jumpToTime_(
          this.mediaElem_.currentTime - (this.mediaElem_.duration/10));
    } else if (evt.keyCode == 38) { // Up - Vol. Up
      this.setVolume_(this.mediaElem_.volume + .1);
    } else if (evt.keyCode == 40) { // Down - Vol. Down
      this.setVolume_(this.mediaElem_.volume - .1);
    }
  }
};

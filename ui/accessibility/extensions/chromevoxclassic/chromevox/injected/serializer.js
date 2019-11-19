// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A global serializer object which returns the current
 * ChromeVox system state.
 */

goog.provide('cvox.Serializer');

goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxEventWatcher');

/**
 * @constructor
 */
cvox.Serializer = function() { };

/**
 * Stores state variables in a provided object.
 *
 * @param {Object} store The object.
 */
cvox.Serializer.prototype.storeOn = function(store) {
  cvox.ChromeVox.storeOn(store);
  cvox.ChromeVoxEventWatcher.storeOn(store);
  cvox.ChromeVox.navigationManager.storeOn(store);
};

/**
 * Updates the object with state variables from an earlier storeOn call.
 *
 * @param {Object} store The object.
 */
cvox.Serializer.prototype.readFrom = function(store) {
  cvox.ChromeVox.readFrom(store);
  cvox.ChromeVoxEventWatcher.readFrom(store);
  cvox.ChromeVox.navigationManager.readFrom(store);
};

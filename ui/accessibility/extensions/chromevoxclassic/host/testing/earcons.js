// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dummy earcons implementation for testing.
 *
 */

goog.provide('cvox.TestEarcons');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.HostFactory');

/**
 * @constructor
 * @extends {cvox.AbstractEarcons}
 */
cvox.TestEarcons = function() {
  cvox.Earcon.call(this);
};
goog.inherits(cvox.TestEarcons, cvox.AbstractEarcons);

/** @override */
cvox.HostFactory.earconsConstructor = cvox.TestEarcons;

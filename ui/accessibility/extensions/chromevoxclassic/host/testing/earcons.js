// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Testing implementation for earcons.
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

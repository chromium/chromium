// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dummy implementation of host.js for testing.
 *
 */

goog.provide('cvox.TestHost');

goog.require('cvox.AbstractHost');
goog.require('cvox.HostFactory');

/**
 * @constructor
 * @extends {cvox.AbstractHost}
 */
cvox.TestHost = function() {
  cvox.AbstractHost.call(this);
};
goog.inherits(cvox.TestHost, cvox.AbstractHost);

/** @override */
cvox.HostFactory.hostConstructor = cvox.TestHost;

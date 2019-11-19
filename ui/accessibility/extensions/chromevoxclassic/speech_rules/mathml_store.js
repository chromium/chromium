// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Speech rule store for mathml and mathjax trees.
 */

goog.provide('cvox.MathmlStore');

goog.require('cvox.MathStore');


/**
 * Rule initialization.
 * @constructor
 * @extends {cvox.MathStore}
 */
cvox.MathmlStore = function() {
  goog.base(this);
};
goog.inherits(cvox.MathmlStore, cvox.MathStore);
goog.addSingletonGetter(cvox.MathmlStore);


/**
 * Adds a new MathML speech rule.
 * @param {string} name Rule name which corresponds to the MathML tag name.
 * @param {string} domain Domain annotation of the rule.
 * @param {string} rule String version of the speech rule.
 */
cvox.MathmlStore.prototype.defineMathmlRule = function(name, domain, rule) {
  this.defineRule(name, domain, rule, 'self::mathml:' + name);
};


/**
 * Adds a new MathML speech rule for the default.default domain.
 * @param {string} name Rule name which corresponds to the MathML tag name.
 * @param {string} rule String version of the speech rule.
 */
cvox.MathmlStore.prototype.defineDefaultMathmlRule = function(name,  rule) {
  this.defineRule(name, 'default.default', rule, 'self::mathml:' + name);
};

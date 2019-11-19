// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Testing stub for MathJax.
 *
 */

goog.provide('cvox.TestMathJax');

goog.require('cvox.AbstractMathJax');
goog.require('cvox.HostFactory');


/**
 * @constructor
 * @extends {cvox.AbstractMathJax}
 */
cvox.TestMathJax = function() {
  goog.base(this);
};
goog.inherits(cvox.TestMathJax, cvox.AbstractMathJax);


/**
 * @override
 */
cvox.TestMathJax.prototype.isMathjaxActive = function(callback) { };


/**
 * @override
 */
cvox.TestMathJax.prototype.getAllJax = function(callback) { };


/**
 * @override
 */
cvox.TestMathJax.prototype.registerSignal = function(
    callback, signal) { };


/**
 * @override
 */
cvox.TestMathJax.prototype.injectScripts = function() { };


/**
 * @override
 */
cvox.TestMathJax.prototype.configMediaWiki = function() { };


/**
 * @override
 */
cvox.TestMathJax.prototype.getTex = function(callback, texNode) { };


/**
 * @override
 */
cvox.TestMathJax.prototype.getAsciiMath = function(callback, asciiMathNode) { };


/** Export platform constructor. */
cvox.HostFactory.mathJaxConstructor =
    cvox.TestMathJax;

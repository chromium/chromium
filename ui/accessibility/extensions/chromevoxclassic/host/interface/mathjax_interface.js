// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface of ChromeVox's bridge to MathJax.
 *
 */

goog.provide('cvox.MathJaxInterface');


/**
 * @interface
 */
cvox.MathJaxInterface = function() { };


/**
 * True if MathJax is injected in a page.
 * @param {function(boolean)} callback A function with the active status as
 *    argument.
 */
cvox.MathJaxInterface.prototype.isMathjaxActive = function(callback) { };


/**
 * Get MathML for all MathJax nodes that already exist by applying the callback
 * to every single MathJax node.
 * @param {function(Node, string)} callback A function taking a node and an id
 * string.
 */
cvox.MathJaxInterface.prototype.getAllJax = function(callback) { };


/**
 * Registers a persistent callback function to be executed by Mathjax on the
 * given signal.
 * @param {function(Node, string)} callback A function taking a node and an id
 * string.
 * @param {string} signal The Mathjax signal to fire the callback.
 */
cvox.MathJaxInterface.prototype.registerSignal = function(callback, signal) { };


/**
 * Injects some minimalistic MathJax script into the page to translate LaTeX
 * expressions.
 */
cvox.MathJaxInterface.prototype.injectScripts = function() { };


/**
 * Loads configurations for MediaWiki pages (e.g., Wikipedia).
 */
cvox.MathJaxInterface.prototype.configMediaWiki = function() { };


/**
 * Get MathML representation of images with tex or latex class if it has an
 * alt text or title.
 * @param {function(Node, string)} callback A function taking a MathML node and
 * an id string.
 * @param {Node} texNode Node with img tag and tex or latex class.
 */
cvox.MathJaxInterface.prototype.getTex = function(callback, texNode) { };


/**
 * Get MathML representation of images that have asciiMath in alt text.
 * @param {function(Node, string)} callback A function taking a MathML node and
 *     an id string.
 * @param {Node} asciiMathNode Node with img tag and class of numberedequation,
 *     inlineformula, or displayformula.
 */
cvox.MathJaxInterface.prototype.getAsciiMath = function(
    callback, asciiMathNode) { };

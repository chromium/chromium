// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implentation of ChromeVox's bridge to MathJax.
 *
 */

goog.provide('cvox.AbstractMathJax');

goog.require('cvox.MathJaxInterface');


/**
 * Creates a new instance.
 * @constructor
 * @implements {cvox.MathJaxInterface}
 */
cvox.AbstractMathJax = function() {
};


/**
 * @override
 */
cvox.AbstractMathJax.prototype.isMathjaxActive = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.getAllJax = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.registerSignal = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.getTex = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.getAsciiMath = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.injectScripts = goog.abstractMethod;


/**
 * @override
 */
cvox.AbstractMathJax.prototype.configMediaWiki = goog.abstractMethod;


/**
 * Get MathML represententations for all images that have latex alt text.
 * @param {function(Node, string)} callback A function taking a MathML node and
 * an id string.
 */
cvox.AbstractMathJax.prototype.getAllTexs = function(callback) {
  var allTexs = document.
      querySelectorAll(cvox.DomUtil.altMathQuerySelector('tex'));
  for (var i = 0, tex; tex = allTexs[i]; i++) {
    this.getTex(callback, tex);
  }
};


/**
 * Get MathML represententations for all images that have asciimath alt text.
 * @param {function(Node, string)} callback A function taking a MathML node and
 * an id string.
 */
cvox.AbstractMathJax.prototype.getAllAsciiMaths = function(callback) {
  var allAsciiMaths = document.
      querySelectorAll(cvox.DomUtil.altMathQuerySelector('asciimath'));
  for (var i = 0, tex; tex = allAsciiMaths[i]; i++) {
    this.getAsciiMath(callback, tex);
  }
};


/**
 * Converts a XML markup string to a DOM node and applies a callback function.
 * The function is generally used in the context of retrieving a MathJax
 * element's MathML representation and converting it from a string. The callback
 * is therefore use by MathJax internally in case the requested MathML
 * representation is not ready yet.
 * @param {function(Node, string)} callback A function taking a node and an id
 * string.
 * @param {string} mml The MathML string.
 * @param {string} id The Mathjax node id.
 */
cvox.AbstractMathJax.prototype.convertMarkupToDom = function(
    callback, mml, id) {
  if (mml) {
    var dp = new DOMParser;
    var cleanMml = mml.replace(/>\s+</g, '><');
    callback(dp.parseFromString(cleanMml, 'text/xml').firstChild, id);
  }
};

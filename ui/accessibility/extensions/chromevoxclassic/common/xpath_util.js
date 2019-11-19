// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with xpaths.
 */


goog.provide('cvox.XpathUtil');


/**
 * Utilities for simplifying working with xpaths
 * @constructor
 */
cvox.XpathUtil = function() {
 };


/**
 * Mapping for some default namespaces.
 * @const
 * @private
 */
cvox.XpathUtil.nameSpaces_ = {
  'xhtml' : 'http://www.w3.org/1999/xhtml',
  'mathml': 'http://www.w3.org/1998/Math/MathML'
};


/**
 * Resolve some default name spaces.
 * @param {string} prefix Namespace prefix.
 * @return {string} The corresponding namespace URI.
 */
cvox.XpathUtil.resolveNameSpace = function(prefix) {
  return cvox.XpathUtil.nameSpaces_[prefix] || null;
};


/**
 * Given an XPath expression and rootNode, it returns an array of children nodes
 * that match. The code for this function was taken from Mihai Parparita's GMail
 * Macros Greasemonkey Script.
 * http://gmail-greasemonkey.googlecode.com/svn/trunk/scripts/gmail-new-macros.user.js
 * @param {string} expression The XPath expression to evaluate.
 * @param {Node} rootNode The HTML node to start evaluating the XPath from.
 * @return {Array} The array of children nodes that match.
 */
cvox.XpathUtil.evalXPath = function(expression, rootNode) {
  try {
    var xpathIterator = rootNode.ownerDocument.evaluate(
      expression,
      rootNode,
      cvox.XpathUtil.resolveNameSpace,
      XPathResult.ORDERED_NODE_ITERATOR_TYPE,
      null); // no existing results
  } catch (err) {
    return [];
  }
  var results = [];
  // Convert result to JS array
  for (var xpathNode = xpathIterator.iterateNext();
       xpathNode;
       xpathNode = xpathIterator.iterateNext()) {
    results.push(xpathNode);
  }
  return results;
};

/**
 * Given a rootNode, it returns an array of all its leaf nodes.
 * @param {Node} rootNode The node to get the leaf nodes from.
 * @return {Array} The array of leaf nodes for the given rootNode.
 */
cvox.XpathUtil.getLeafNodes = function(rootNode) {
  try {
    var xpathIterator = rootNode.ownerDocument.evaluate(
      './/*[count(*)=0]',
      rootNode,
      null, // no namespace resolver
      XPathResult.ORDERED_NODE_ITERATOR_TYPE,
      null); // no existing results
  } catch (err) {
    return [];
  }
  var results = [];
  // Convert result to JS array
  for (var xpathNode = xpathIterator.iterateNext();
       xpathNode;
       xpathNode = xpathIterator.iterateNext()) {
    results.push(xpathNode);
  }
  return results;
};

/**
 * Returns whether or not xpath is supported.
 * @return {boolean} True if xpath is supported.
 */
cvox.XpathUtil.xpathSupported = function() {
  if (typeof(XPathResult) == 'undefined') {
    return false;
  }
  return true;
};


/**
 * Given an XPath expression and rootNode, it evaluates the XPath expression as
 * a boolean type and returns the result.
 * @param {string} expression The XPath expression to evaluate.
 * @param {Node} rootNode The HTML node to start evaluating the XPath from.
 * @return {boolean} The result of evaluating the xpath expression.
 */
cvox.XpathUtil.evaluateBoolean = function(expression, rootNode) {
  try {
    var xpathResult = rootNode.ownerDocument.evaluate(
        expression,
        rootNode,
        cvox.XpathUtil.resolveNameSpace,
        XPathResult.BOOLEAN_TYPE,
        null); // no existing results
  } catch (err) {
    return false;
  }
  return xpathResult.booleanValue;
};


/**
 * Given an XPath expression and rootNode, it evaluates the XPath expression as
 * a string type and returns the result.
 * @param {string} expression The XPath expression to evaluate.
 * @param {Node} rootNode The HTML node to start evaluating the XPath from.
 * @return {string} The result of evaluating the Xpath expression.
 */
cvox.XpathUtil.evaluateString = function(expression, rootNode) {
  try {
    var xpathResult = rootNode.ownerDocument.evaluate(
        expression,
        rootNode,
        cvox.XpathUtil.resolveNameSpace,
        XPathResult.STRING_TYPE,
        null); // no existing results
  } catch (err) {
    return '';
  }
  return xpathResult.stringValue;
};

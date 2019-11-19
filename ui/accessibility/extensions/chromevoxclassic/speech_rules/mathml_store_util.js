// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for mathml and mathjax rule store.
 */

goog.provide('cvox.MathmlStoreUtil');

goog.require('cvox.MathUtil');
goog.require('cvox.TraverseMath');


/**
 * Retrieves MathML sub element with same id as MathJax node.
 * @param {!Node} inner A node internal to a MathJax node.
 * @return {Node} The internal MathML node corresponding to the MathJax node.
 */
cvox.MathmlStoreUtil.matchMathjaxToMathml = function(inner) {
  var mml = cvox.TraverseMath.getInstance().activeMathmlHost;
  return mml.querySelector('#' + inner.id);
};


/**
 * Retrieve an extender symbol for a given node.
 * @param {!Node} jax The MathJax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.retrieveMathjaxExtender = function(jax) {
  var ext = cvox.MathmlStoreUtil.matchMathjaxToMathml(jax);
  if (ext) {
    return [ext];
  }
  return [];
};


/**
 * Retrieve an extender symbol for a given node.
 * @param {!Node} jax The MathJax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.retrieveMathjaxLeaf = function(jax) {
  var leaf = cvox.MathmlStoreUtil.matchMathjaxToMathml(jax);
  if (leaf) {
    return [leaf];
  }
  return [];
};


/**
 * For a given MathJax node it returns the equivalent MathML node,
 * if it is of the right tag.
 * @param {!Node} jax The Mathjax node.
 * @param {!string} tag The required tag.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.checkMathjaxTag = function(jax, tag) {
  var node = cvox.MathmlStoreUtil.matchMathjaxToMathml(jax);
  if (node && node.tagName.toUpperCase() == tag) {
    return [node];
  }
  return [];
};


/**
 * Returns MathML node if MathJax is munder.
 * @param {!Node} jax The Mathjax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.checkMathjaxMunder = function(jax) {
  return cvox.MathmlStoreUtil.checkMathjaxTag(jax, 'MUNDER');
};


/**
 * Returns MathML node if MathJax is mover.
 * @param {!Node} jax The Mathjax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.checkMathjaxMover = function(jax) {
  return cvox.MathmlStoreUtil.checkMathjaxTag(jax, 'MOVER');
};


/**
 * Returns MathML node if MathJax is msub.
 * @param {!Node} jax The Mathjax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.checkMathjaxMsub = function(jax) {
  return cvox.MathmlStoreUtil.checkMathjaxTag(jax, 'MSUB');
};


/**
 * Returns MathML node if MathJax is msup.
 * @param {!Node} jax The Mathjax node.
 * @return {Array<Node>} The resulting node list.
 */
cvox.MathmlStoreUtil.checkMathjaxMsup = function(jax) {
  return cvox.MathmlStoreUtil.checkMathjaxTag(jax, 'MSUP');
};


/**
 * Constructs a closure that returns separators for an MathML mfenced
 * expression.
 * Separators in MathML are represented by a list and used up one by one
 * until the final element is used as the default.
 * Example: a b c d e  and separators [+,-,*]
 * would result in a + b - c * d * e.
 * @param {string} separators String representing a list of mfenced separators.
 * @return {function(): string|null} A closure that returns the next separator
 * for an mfenced expression starting with the first node in nodes.
 */
cvox.MathmlStoreUtil.nextSeparatorFunction = function(separators) {
  if (separators) {
    // Mathjax does not expand empty separators.
    if (separators.match(/^\s+$/)) {
      return null;
    } else {
      var sepList = separators.replace(/\s/g, '')
          .split('')
              .filter(function(x) {return x;});
    }
  } else {
    // When no separator is given MathML uses comma as default.
    var sepList = [','];
  }

  return function() {
    if (sepList.length > 1) {
      return sepList.shift();
    }
    return sepList[0];
  };
};


/**
 * Computes the correct separators for each node.
 * @param {Array<Node>} nodes A node array.
 * @param {string} context A context string.
 * @return {function(): string} A closure that returns the next separator for an
 * mfenced expression starting with the first node in nodes.
 */
cvox.MathmlStoreUtil.mfencedSeparators = function(nodes, context) {
  var nextSeparator = cvox.MathmlStoreUtil.nextSeparatorFunction(context);
  return function() {
    return nextSeparator ? nextSeparator() : '';
  };
};


/**
 * Iterates over the list of content nodes of the parent of the given nodes.
 * @param {Array<Node>} nodes A node array.
 * @param {string} context A context string.
 * @return {function(): string} A closure that returns the content of the next
 *     content node. Returns only context string if list is exhausted.
 */
cvox.MathmlStoreUtil.contentIterator = function(nodes, context) {
  if (nodes.length > 0) {
    var contentNodes = cvox.XpathUtil.evalXPath('../../content/*', nodes[0]);
  } else {
    var contentNodes = [];
  }
  return function() {
    var content = contentNodes.shift();
    return context + (content ? content.textContent : '');
  };
};

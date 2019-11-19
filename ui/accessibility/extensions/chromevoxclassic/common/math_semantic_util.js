// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for semantic tree computations.
 */

goog.provide('cvox.SemanticUtil');


/**
 * @constructor
 */
cvox.SemanticUtil = function() { };


/**
 * Merges keys of objects into an array.
 * @param {...Object<string>} objects Optional objects.
 * @return {Array<string>} Array of all keys of the objects.
 */
cvox.SemanticUtil.objectsToKeys = function(objects) {
  objects = Array.prototype.slice.call(arguments, 0);
  var keys = [];
  return keys.concat.apply(keys, objects.map(Object.keys));
};


/**
 * Merges values of objects into an array.
 * @param {...Object<string>} objects Optional objects.
 * @return {Array<string>} Array of all values of the objects.
 */
cvox.SemanticUtil.objectsToValues = function(objects) {
  objects = Array.prototype.slice.call(arguments, 0);
  var result = [];
  var collectValues = function(obj) {
    for (var key in obj) {
      result.push(obj[key]);
    }
  };
  objects.forEach(collectValues);
  return result;
};


/**
 * Transforms a unicode character into numeric representation. Returns null if
 * the input string is not a valid unicode character.
 * @param {string} unicode Character.
 * @return {?number} The decimal representation if it exists.
 */
cvox.SemanticUtil.unicodeToNumber = function(unicode) {
  if (!unicode || unicode.length > 2) {
    return null;
  }
  // Treating surrogate pairs.
  if (unicode.length == 2) {
    var hi = unicode.charCodeAt(0);
    var low = unicode.charCodeAt(1);
    if (0xD800 <= hi && hi <= 0xDBFF && !isNaN(low)) {
      return ((hi - 0xD800) * 0x400) + (low - 0xDC00) + 0x10000;
    }
    return null;
  }
  return unicode.charCodeAt(0);
};


/**
 * Transforms a numberic representation of a unicode character into its
 * corresponding string.
 * @param {number} number Unicode point.
 * @return {string} The string representation.
 */
cvox.SemanticUtil.numberToUnicode = function(number) {
  if (number >= 0x10000) {
    var hi = (number - 0x10000) / 0x0400 + 0xD800;
    var lo = (number - 0x10000) % 0x0400 + 0xDC00;
    return String.fromCharCode(hi, lo);
  }
  return String.fromCharCode(number);
};


/**
 * Returns the tagname of an element node in upper case.
 * @param {Element} node The node.
 * @return {string} The node's tagname.
 */
cvox.SemanticUtil.tagName = function(node) {
  return node.tagName.toUpperCase();
};


/**
 * List of MathML Tags that are to be ignored.
 * @type {Array<string>}
 * @const
 */
cvox.SemanticUtil.IGNORETAGS = [
  'MERROR', 'MPHANTOM', 'MSPACE', 'MACTION', 'MALIGNGROUP', 'MALIGNMARK',
  'MACTION'
];


/**
 * List of MathML Tags to be ignore if they have no children.
 * @type {Array<string>}
 * @const
 */
cvox.SemanticUtil.EMPTYTAGS = ['MATH', 'MROW', 'MPADDED', 'MSTYLE'];


/**
 * Removes elements from a list of MathML nodes that are either to be ignored or
 * ignored if they have empty children.
 * Observe that this is currently not recursive, i.e. will not take care of
 * pathological cases, where content is hidden in incorrectly used tags!
 * @param {Array<Element>} nodes The node list to be cleaned.
 * @return {Array<Element>} The cleansed list.
 */
cvox.SemanticUtil.purgeNodes = function(nodes) {
  var nodeArray = [];
  for (var i = 0, node; node = nodes[i]; i++) {
    var tagName = cvox.SemanticUtil.tagName(node);
    if (cvox.SemanticUtil.IGNORETAGS.indexOf(tagName) != -1) continue;
    if (cvox.SemanticUtil.EMPTYTAGS.indexOf(tagName) != -1 &&
        node.childNodes.length == 0)
    continue;
    nodeArray.push(node);
  }
  return nodeArray;
};

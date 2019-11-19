// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DOM utility functions to aid in math expressions navigation.
 */

goog.provide('cvox.MathUtil');

goog.require('cvox.ChromeVox');
goog.require('cvox.XpathUtil');


/**
 * Checks if a node is in a given class of MathML nodes.
 * @private
 * @param {!Node} node The node to test.
 * @param {Array<string>} tags List of tag names.
 * @return {boolean} True if node has a tag name included in tags.
 */
cvox.MathUtil.isMathmlNodeOfClass_ = function(node, tags) {
  return tags.indexOf(node.tagName.toUpperCase()) != -1;
};


/**
 * Checks if a node is in a given class of MathJax nodes.
 * @private
 * @param {!Node} node The node to test.
 * @param {Array<string>} tags List of tag names.
 * @return {boolean} True if node has a tag name included in tags.
 */
cvox.MathUtil.isMathjaxNodeOfClass_ = function(node, tags) {
  if (node.tagName == 'SPAN') {
    var classes = node.className.split(' ');
    return classes.some(function(x)
                        {return tags.indexOf(x.toUpperCase()) != -1;});
  }
  return false;
};


/**
 * Checks if a node is an element node that belongs to a given class
 * of MathML or MathJax nodes.
 * @private
 * @param {!Node} node The node to test.
 * @param {Array<string>} tags List of tag names.
 * @return {boolean} True if node has a tag name included in tags.
 */
cvox.MathUtil.isMathNodeOfClass_ = function(node, tags) {
  return (node.nodeType == Node.ELEMENT_NODE &&
          (cvox.MathUtil.isMathmlNodeOfClass_(node, tags) ||
           cvox.MathUtil.isMathjaxNodeOfClass_(node, tags)));
};


/**
 * Array of MathML Token Elements.
 * @type {!Array<string>}
 */
cvox.MathUtil.TOKEN_LIST = ['MI', 'MN', 'MO', 'MTEXT', 'MSPACE', 'MS'];


/**
 *  Checks if an element of a math expression is a Token Element.
 * Token elements are the following:
 * <mi> identifier.
 * <mn> number.
 * <mo> operator, fence, or separator.
 * <mtext> text.
 * <mspace> space.
 * <ms> string literal.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a token.
 */
cvox.MathUtil.isToken = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element, cvox.MathUtil.TOKEN_LIST);
};


/**
 * Array of MathML Layout Schemata.
 * @type {!Array<string>}
 */
cvox.MathUtil.LAYOUT_LIST = ['MROW', 'MFRAC', 'MSQRT', 'MROOT', 'MSTYLE',
                             'MERROR', 'MPADDED', 'MPHANTOM', 'MFENCED',
                             'MENCLOSE'];


/**
 *  Checks if an element of a math expression is a Layout Schema.
 * Layout elements are the following:
 * <mrow> group any number of sub-expressions horizontally
 * <mfrac> form a fraction from two sub-expressions
 * <msqrt> form a square root (radical without an index)
 * <mroot> form a radical with specified index
 * <mstyle> style change
 * <merror> enclose a syntax error message from a preprocessor
 * <mpadded> adjust space around content
 * <mphantom> make content invisible but preserve its size
 * <mfenced> surround content with a pair of fences
 * <menclose> enclose content with a stretching symbol such as a long
 * division sign.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a layout schema.
 */
cvox.MathUtil.isLayout = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element, cvox.MathUtil.LAYOUT_LIST);
};


/**
 * Array of MathML Script Schemata.
 * @type {!Array<string>}
 */
cvox.MathUtil.SCRIPT_LIST = ['MSUB', 'MSUP', 'MSUBSUP', 'MUNDER', 'MOVER',
                             'MUNDEROVER', 'MMULTISCRIPTS', 'MPRESCRIPTS'];


/**
 *  Checks if an element of a math expression is a Script Schema.
 * Script elements are the following:
 * <msub> attach a subscript to a base.
 * <msup> attach a superscript to a base.
 * <msubsup> attach a subscript-superscript pair to a base.
 * <munder> attach an underscript to a base.
 * <mover> attach an overscript to a base.
 * <munderover> attach an underscript-overscript pair to a base.
 * <mmultiscripts> attach prescripts and tensor indices to a base.
 * Prescripts are optional.
 * <mprescripts> two elements prescripts of mmultiscripts. Only makes sense
 * in that environment (although not illegal outside)!  Two
 * arguments mandatory (can be <none/>).
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a script schema.
 */
cvox.MathUtil.isScript = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element, cvox.MathUtil.SCRIPT_LIST);
};


/**
 * Array of MathML Table and Matrix tokens.
 * @type {!Array<string>}
 */
cvox.MathUtil.TABLES_LIST = ['MTABLE', 'MLABELEDTR', 'MTR', 'MTD',
                             'MALIGNGROUP', 'MALIGNMARK'];


/**
 *  Checks if an element of a math expression is a Tables Schema.
 * Tables elements are the following:
 * <mtable> table or matrix.
 * <mlabeledtr> row in a table or matrix with a label or equation number.
 * <mtr> row in a table or matrix.
 * <mtd> one entry in a table or matrix.
 * <maligngroup> and
 * <malignmark> alignment markers.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a tables schema.
 */
cvox.MathUtil.isTables = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element, cvox.MathUtil.TABLES_LIST);
};


/**
 * Array of MathML Elementary Layout Schemata.
 * @type {!Array<string>}
 */
cvox.MathUtil.ELEMENTARY_LIST = ['MSTACK', 'MLONGDIV', 'MSGROUP', 'MSROW',
                                 'MSCARRIES', 'MSCARRY', 'MSLINE'];


/**
 *  Checks if an element of a math expression is a Elementary Schema.
 * Elementary elements are the following:
 * <mstack> columns of aligned characters.
 * <mlongdiv> similar to msgroup, with the addition of a divisor and result.
 * <msgroup> a group of rows in an mstack that are shifted by similar amounts.
 * <msrow> a row in an mstack.
 * <mscarries> row in an mstack that whose contents represent carries
 *             or borrows.
 * <mscarry> one entry in an mscarries.
 * <msline> horizontal line inside of mstack.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a elementary schema.
 */
cvox.MathUtil.isElementary = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element,
                                          cvox.MathUtil.ELEMENTARY_LIST);
};


/**
 * Array of all valid tags in a MathML expression.
 * This is a union of all other token lists.
 * @type {!Array<string>}
 */
cvox.MathUtil.MATHML_TAG_LIST = [cvox.MathUtil.TOKEN_LIST,
                                 cvox.MathUtil.LAYOUT_LIST,
                                 cvox.MathUtil.SCRIPT_LIST,
                                 cvox.MathUtil.TABLES_LIST,
                                 cvox.MathUtil.ELEMENTARY_LIST].reduce(
                                     function(x, y) { return x.concat(y); });


/**
 * Checks if a node is valid element of a MathML expression.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element has a valid MathML tag.
 */
cvox.MathUtil.isMathmlTag = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element,
                                          cvox.MathUtil.MATHML_TAG_LIST);
};


/**
 * Array of MathML Whitespace and Alignment tokens.
 * These are elements that can occur in the other token lists.
 * @type {!Array<string>}
 */
cvox.MathUtil.WHITESPACE_LIST = ['MSROW', 'MROW', 'MSPACE',
                                 'MPHANTOM', 'MPADDED'];


/**
 * Checks if an element of a math expression is whitespace or an
 * alignment marker.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a whitespace node.
 */
cvox.MathUtil.isWhitespace = function(element) {
  return cvox.MathUtil.isMathNodeOfClass_(element,
                                          cvox.MathUtil.WHITESPACE_LIST);
};


/**
 * Checks if an element of a math expression is a legal mathml markup element
 * but not a whitespace or an alignment marker.
 * @param {!Node} element The element of the math expression.
 * @return {boolean} True if element is a non-whitespace node.
 */
cvox.MathUtil.isNotWhitespace = function(element) {
  return (cvox.MathUtil.isMathmlTag(element) &&
          !cvox.MathUtil.isWhitespace(element));
};


/**
 * Computes the union of two arrays (not in a strictly set theoretical sense
 * as all duplicate elements in either array still remain as duplicates!).
 * @param {Array} a An array.
 * @param {Array} b Another array.
 * @return {Array} Union of a and b.
 */
cvox.MathUtil.union = function(a, b) {
  return a.concat(b.filter(function(x) {return a.indexOf(x) < 0;}));
};

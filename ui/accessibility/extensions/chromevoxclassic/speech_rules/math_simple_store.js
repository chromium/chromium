// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Rule stores for the basic components of math expressions:
 *    Unicode symbols and functions.
 *
 *    The idea of these stores is to provide a more efficient data structure to
 *    look up rules in the background page than the usual flat array of rules
 *    implemented by other stores.
 *
 */

goog.provide('cvox.MathCompoundStore');
goog.provide('cvox.MathSimpleStore');

goog.require('cvox.MathStore');
goog.require('cvox.SpeechRule');

/**
 * A base store for simple Math objects.
 * @constructor
 * @extends {cvox.MathStore}
 */
cvox.MathSimpleStore = function() {
  goog.base(this);
 };
goog.inherits(cvox.MathSimpleStore, cvox.MathStore);


/**
 * Turns a domain mapping from its JSON representation containing simple strings
 * only into a list of speech rules.
 * @param {string} name Name for the rules.
 * @param {string} str String for precondition and constraints.
 * @param {Object<Object<string>>} mapping Simple string
 *     mapping.
 */
cvox.MathSimpleStore.prototype.defineRulesFromMappings = function(
  name, str, mapping) {
  for (var domain in mapping) {
    for (var style in mapping[domain]) {
      var content = mapping[domain][style];
      var cstr = 'self::text() = "' + str + '"';
      var rule = this.defineRule(
          name, domain + '.' + style, '[t] "' + content + '"',
          'self::text()', cstr);
    }
  }
};


/**
 * A compound store for simple Math objects.
 * @constructor
 */
cvox.MathCompoundStore = function() {
  /**
   * A set of efficient substores.
   * @type {Object<cvox.MathStore>}
   * @private
   */
  this.subStores_ = {};
};
goog.addSingletonGetter(cvox.MathCompoundStore);


/**
 * Function creates a rule store in the compound store for a particular string,
 * and populates it with a set of rules.
 * @param {string} name Name of the rule.
 * @param {string} str String used as key to refer to the rule store
 * precondition and constr
 * @param {Object} mappings JSON representation of mappings from styles and
 *     domains to strings, from which the speech rules will be computed.
 */
cvox.MathCompoundStore.prototype.defineRules = function(name, str, mappings) {
  var store = new cvox.MathSimpleStore();
  store.defineRulesFromMappings(name, str, mappings);
  this.subStores_[str] = store;
};


/**
 * Makes a speech rule for Unicode characters from its JSON representation.
 * @param {Object} json JSON object of the speech rules.
 */
cvox.MathCompoundStore.prototype.addSymbolRules = function(json) {
  var key = cvox.MathSimpleStore.parseUnicode_(json['key']);
  this.defineRules(json['key'], key, json['mappings']);
};


/**
 * Makes a speech rule for Unicode characters from its JSON representation.
 * @param {Object} json JSON object of the speech rules.
 */
cvox.MathCompoundStore.prototype.addFunctionRules = function(json) {
  var names = json['names'];
  var mappings = json['mappings'];
  for (var j = 0, name; name = names[j]; j++) {
    this.defineRules(name, name, mappings);
  }
};


/**
 * Retrieves a rule for the given node if one exists.
 * @param {Node} node A node.
 * @param {!cvox.SpeechRule.DynamicCstr} dynamic Additional dynamic
 *     constraints. These are matched against properties of a rule.
 * @return {cvox.SpeechRule} The speech rule if it exists.
 */
cvox.MathCompoundStore.prototype.lookupRule = function(node, dynamic) {
  var store = this.subStores_[node.textContent];
  if (store) {
    return store.lookupRule(node, dynamic);
  }
  return null;
};


/**
 * Looks up a rule for a given string and executes its actions.
 * @param {string} text The text to be translated.
 * @param {!cvox.SpeechRule.DynamicCstr} dynamic Additional dynamic
 *     constraints. These are matched against properties of a rule.
 * @return {!string} The string resulting from the action of speech rule.
 */
cvox.MathCompoundStore.prototype.lookupString = function(text, dynamic) {
  var textNode = document.createTextNode(text);
  var rule = this.lookupRule(textNode, dynamic);
  if (!rule) {
    return '';
  }
  return rule.action.components
    .map(function(comp) {
           return comp.content.slice(1, -1);})
    .join(' ');
};


/**
 * Get a set of all dynamic constraint values.
 * @return {!Object<cvox.SpeechRule.DynamicCstrAttrib, Array<string>>} The
 *     object with all annotations.
 */
cvox.MathCompoundStore.prototype.getDynamicConstraintValues = function() {
  var newCstr = {};
  for (var store in this.subStores_) {
    var cstr = this.subStores_[store].getDynamicConstraintValues();
    for (var key in cstr) {
      var set = newCstr[key];
      if (set) {
        newCstr[key] = cvox.MathUtil.union(set, cstr[key]);
        } else {
        newCstr[key] = cstr[key];
        }
    }
  }
  return newCstr;
};


/**
 * Parses a string with a hex representatino of a unicode code point into the
 * corresponding unicode character.
 * @param {string} number The code point to be parsed.
 * @return {string} The unicode character.
 * @private
 */
cvox.MathSimpleStore.parseUnicode_ = function(number) {
  var keyValue = parseInt(number, 16);
  if (keyValue < 0x10000) {
    return String.fromCharCode(keyValue);
  }
  keyValue -= 0x10000;
  var hiSurrogate = (keyValue >> 10) + 0xD800;
  var lowSurrogate = (keyValue & 0x3FF) + 0xDC00;
  return String.fromCharCode(hiSurrogate, lowSurrogate);
};

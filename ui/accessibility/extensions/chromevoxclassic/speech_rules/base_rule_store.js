// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for all speech rule stores.
 *
 * The base rule store implements some basic functionality that is common to
 * most speech rule stores.
 */

goog.provide('cvox.BaseRuleStore');

goog.require('cvox.MathUtil');
goog.require('cvox.SpeechRule');
goog.require('cvox.SpeechRuleEvaluator');
goog.require('cvox.SpeechRuleFunctions');
goog.require('cvox.SpeechRuleStore');


/**
 * @constructor
 * @implements {cvox.SpeechRuleEvaluator}
 * @implements {cvox.SpeechRuleStore}
 */
cvox.BaseRuleStore = function() {
  /**
   * Set of custom query functions for the store.
   * @type {cvox.SpeechRuleFunctions.CustomQueries}
   */
  this.customQueries = new cvox.SpeechRuleFunctions.CustomQueries();

  /**
   * Set of custom strings for the store.
   * @type {cvox.SpeechRuleFunctions.CustomStrings}
   */
  this.customStrings = new cvox.SpeechRuleFunctions.CustomStrings();

  /**
   * Set of context functions for the store.
   * @type {cvox.SpeechRuleFunctions.ContextFunctions}
   */
  this.contextFunctions = new cvox.SpeechRuleFunctions.ContextFunctions();

  /**
   * Set of speech rules in the store.
   * @type {!Array<cvox.SpeechRule>}
   * @private
   */
  this.speechRules_ = [];

  /**
   * A priority list of dynamic constraint attributes.
   * @type {!Array<cvox.SpeechRule.DynamicCstrAttrib>}
   */
  this.dynamicCstrAttribs = [cvox.SpeechRule.DynamicCstrAttrib.STYLE];

  /**
   * List of TTS properties overridden by the store when it is active.
   * @type {!Array<string>}
   */
  this.defaultTtsProps = [];
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.lookupRule = function(node, dynamic) {
  if (!node ||
      (node.nodeType != Node.ELEMENT_NODE && node.nodeType != Node.TEXT_NODE)) {
    return null;
  }
  var matchingRules = this.speechRules_.filter(
      goog.bind(
          function(rule) {
            return this.testDynamicConstraints(dynamic, rule) &&
                this.testPrecondition_(/** @type {!Node} */ (node), rule);},
          this));
  return (matchingRules.length > 0) ?
    this.pickMostConstraint_(dynamic, matchingRules) : null;
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.defineRule = function(
    name, dynamic, action, prec, cstr) {
  try {
    var postc = cvox.SpeechRule.Action.fromString(action);
    var cstrList = Array.prototype.slice.call(arguments, 4);
    var fullPrec = new cvox.SpeechRule.Precondition(prec, cstrList);
    var dynamicCstr = {};
    dynamicCstr[cvox.SpeechRule.DynamicCstrAttrib.STYLE] = dynamic;
    var rule = new cvox.SpeechRule(name, dynamicCstr, fullPrec, postc);
  } catch (err) {
    if (err.name == 'RuleError') {
      console.log('Rule Error ', prec, '(' + dynamic + '):', err.message);
      return null;
    }
    else {
      throw err;
    }
  }
  this.addRule(rule);
  return rule;
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.addRule = function(rule) {
  this.speechRules_.unshift(rule);
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.deleteRule = function(rule) {
  var index = this.speechRules_.indexOf(rule);
  if (index != -1) {
    this.speechRules_.splice(index, 1);
  }
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.findRule = function(pred) {
  for (var i = 0, rule; rule = this.speechRules_[i]; i++) {
    if (pred(rule)) {
      return rule;
    }
  }
  return null;
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.findAllRules = function(pred) {
  return this.speechRules_.filter(pred);
};


/**
 * @override
 */
cvox.BaseRuleStore.prototype.evaluateDefault = function(node) {
  return [new cvox.NavDescription({'text': node.textContent})];
};


/**
 * Test the applicability of a speech rule in debugging mode.
 * @param {string} name Rule to debug.
 * @param {!Node} node DOM node to test applicability of given rule.
 */
cvox.BaseRuleStore.prototype.debugSpeechRule = goog.abstractMethod;


/**
 * Function to initialize the store with speech rules. It is called by the
 * speech rule engine upon parametrization with this store. The function allows
 * us to define sets of rules in separate files while depending on functionality
 * that is defined in the rule store.
 * Essentially it is a way of getting around dependencies.
 */
cvox.BaseRuleStore.prototype.initialize = goog.abstractMethod;


/**
 * Removes duplicates of the given rule from the rule store. Thereby duplicates
 * are identified by having the same precondition and dynamic constraint.
 * @param {cvox.SpeechRule} rule The rule.
 */
cvox.BaseRuleStore.prototype.removeDuplicates = function(rule) {
  for (var i = this.speechRules_.length - 1, oldRule;
       oldRule = this.speechRules_[i]; i--) {
         if (oldRule != rule &&
             cvox.BaseRuleStore.compareDynamicConstraints_(
                 oldRule.dynamicCstr, rule.dynamicCstr) &&
                     cvox.BaseRuleStore.comparePreconditions_(oldRule, rule)) {
           this.speechRules_.splice(i, 1);
         }
       }
};


// TODO (sorge) These should move into the speech rule functions.
/**
 * Checks if we have a custom query and applies it. Otherwise returns null.
 * @param {!Node} node The initial node.
 * @param {string} funcName A function name.
 * @return {Array<Node>} The list of resulting nodes.
 */
cvox.BaseRuleStore.prototype.applyCustomQuery = function(
    node, funcName) {
  var func = this.customQueries.lookup(funcName);
  return func ? func(node) : null;
};


/**
 * Applies either an Xpath selector or a custom query to the node
 * and returns the resulting node list.
 * @param {!Node} node The initial node.
 * @param {string} expr An Xpath expression string or a name of a custom
 *     query.
 * @return {Array<Node>} The list of resulting nodes.
 */
cvox.BaseRuleStore.prototype.applySelector = function(node, expr) {
  var result = this.applyCustomQuery(node, expr);
  return result || cvox.XpathUtil.evalXPath(expr, node);
};


/**
 * Applies either an Xpath selector or a custom query to the node
 * and returns the first result.
 * @param {!Node} node The initial node.
 * @param {string} expr An Xpath expression string or a name of a custom
 *     query.
 * @return {Node} The resulting node.
 */
cvox.BaseRuleStore.prototype.applyQuery = function(node, expr) {
  var results = this.applySelector(node, expr);
  if (results.length > 0) {
    return results[0];
  }
  return null;
};


/**
 * Applies either an Xpath selector or a custom query to the node and returns
 * true if the application yields a non-empty result.
 * @param {!Node} node The initial node.
 * @param {string} expr An Xpath expression string or a name of a custom
 *     query.
 * @return {boolean} True if application was successful.
 */
cvox.BaseRuleStore.prototype.applyConstraint = function(node, expr) {
  var result = this.applyQuery(node, expr);
  return !!result || cvox.XpathUtil.evaluateBoolean(expr, node);
};


/**
 * Tests whether a speech rule satisfies a set of dynamic constraints.
 * @param {!cvox.SpeechRule.DynamicCstr} dynamic Dynamic constraints.
 * @param {cvox.SpeechRule} rule The rule.
 * @return {boolean} True if the preconditions apply to the node.
 * @protected
 */
cvox.BaseRuleStore.prototype.testDynamicConstraints = function(
    dynamic, rule) {
  // We allow a default value for each dynamic constraints attribute.
  // The idea is that when we can not find a speech rule matching the value for
  // a particular attribute in the dynamic constraintwe choose the one that has
  // the value 'default'.
  var allKeys = /** @type {Array<cvox.SpeechRule.DynamicCstrAttrib>} */ (
      Object.keys(dynamic));
  return allKeys.every(
      function(key) {
        return dynamic[key] == rule.dynamicCstr[key] ||
            rule.dynamicCstr[key] == 'default';
      });
};


/**
 * Get a set of all dynamic constraint values.
 * @return {!Object<cvox.SpeechRule.DynamicCstrAttrib, Array<string>>} The
 *     object with all annotations.
 */
cvox.BaseRuleStore.prototype.getDynamicConstraintValues = function() {
  var result = {};
  for (var i = 0, rule; rule = this.speechRules_[i]; i++) {
    for (var key in rule.dynamicCstr) {
      var newKey = [rule.dynamicCstr[key]];
      if (result[key]) {
        result[key] = cvox.MathUtil.union(result[key], newKey);
      } else {
        result[key] = newKey;
      }
    }
  }
  return result;
};


/**
 * Counts how many dynamic constraint values match exactly in the order
 * specified by the store.
 * @param {cvox.SpeechRule.DynamicCstr} dynamic Dynamic constraints.
 * @param {cvox.SpeechRule} rule The speech rule to match.
 * @return {number} The number of matching dynamic constraint values.
 * @private
 */
cvox.BaseRuleStore.prototype.countMatchingDynamicConstraintValues_ = function(
    dynamic, rule) {
  var result = 0;
  for (var i = 0, key; key = this.dynamicCstrAttribs[i]; i++) {
    if (dynamic[key] == rule.dynamicCstr[key]) {
      result++;
    } else break;
  }
  return result;
};


/**
 * Picks the result of the most constraint rule by prefering those:
 * 1) that best match the dynamic constraints.
 * 2) with the most additional constraints.
 * @param {cvox.SpeechRule.DynamicCstr} dynamic Dynamic constraints.
 * @param {!Array<cvox.SpeechRule>} rules An array of rules.
 * @return {cvox.SpeechRule} The most constraint rule.
 * @private
 */
cvox.BaseRuleStore.prototype.pickMostConstraint_ = function(dynamic, rules) {
  rules.sort(goog.bind(
      function(r1, r2) {
        var count1 = this.countMatchingDynamicConstraintValues_(dynamic, r1);
        var count2 = this.countMatchingDynamicConstraintValues_(dynamic, r2);
        // Rule one is a better match, don't swap.
        if (count1 > count2) {
          return -1;
        }
        // Rule two is a better match, swap.
        if (count2 > count1) {
          return 1;
        }
        // When same number of dynamic constraint attributes matches for
        // both rules, compare length of static constraints.
        return (r2.precondition.constraints.length -
            r1.precondition.constraints.length);},
      this));
  return rules[0];
};


/**
 * Test the precondition of a speech rule.
 * @param {!Node} node on which to test applicability of the rule.
 * @param {cvox.SpeechRule} rule The rule to be tested.
 * @return {boolean} True if the preconditions apply to the node.
 * @private
 */
cvox.BaseRuleStore.prototype.testPrecondition_ = function(node, rule) {
  var prec = rule.precondition;
  return this.applyQuery(node, prec.query) === node &&
      prec.constraints.every(
          goog.bind(function(cstr) {
                      return this.applyConstraint(node, cstr);},
                    this));
};


// TODO (sorge) Define the following methods directly on the dynamic constraint
//     and precondition classes, respectively.
/**
 * Compares two dynamic constraints and returns true if they are equal.
 * @param {cvox.SpeechRule.DynamicCstr} cstr1 First dynamic constraints.
 * @param {cvox.SpeechRule.DynamicCstr} cstr2 Second dynamic constraints.
 * @return {boolean} True if the dynamic constraints are equal.
 * @private
 */
cvox.BaseRuleStore.compareDynamicConstraints_ = function(
    cstr1, cstr2) {
  if (Object.keys(cstr1).length != Object.keys(cstr2).length) {
    return false;
  }
  for (var key in cstr1) {
    if (!cstr2[key] || cstr1[key] !== cstr2[key]) {
      return false;
    }
  }
  return true;
};


/**
 * Compares two static constraints (i.e., lists of precondition constraints) and
 * returns true if they are equal.
 * @param {Array<string>} cstr1 First static constraints.
 * @param {Array<string>} cstr2 Second static constraints.
 * @return {boolean} True if the static constraints are equal.
 * @private
 */
cvox.BaseRuleStore.compareStaticConstraints_ = function(
    cstr1, cstr2) {
  if (cstr1.length != cstr2.length) {
    return false;
  }
  for (var i = 0, cstr; cstr = cstr1[i]; i++) {
    if (cstr2.indexOf(cstr) == -1) {
      return false;
    }
  }
  return true;
};


/**
 * Compares the preconditions of two speech rules.
 * @param {cvox.SpeechRule} rule1 The first speech rule.
 * @param {cvox.SpeechRule} rule2 The second speech rule.
 * @return {boolean} True if the preconditions are equal.
 * @private
 */
cvox.BaseRuleStore.comparePreconditions_ = function(rule1, rule2) {
  var prec1 = rule1.precondition;
  var prec2 = rule2.precondition;
  if (prec1.query != prec2.query) {
    return false;
    }
  return cvox.BaseRuleStore.compareStaticConstraints_(
      prec1.constraints, prec2.constraints);
};

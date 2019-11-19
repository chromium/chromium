// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Rule store for math syntax tree nodes.
 */

goog.provide('cvox.MathStore');

goog.require('cvox.AbstractTts');
goog.require('cvox.BaseRuleStore');
goog.require('cvox.ChromeVox');
goog.require('cvox.NavMathDescription');
goog.require('cvox.SpeechRule');
goog.require('cvox.TraverseMath');


/**
 * A store for Math rules.
 * @constructor
 * @extends {cvox.BaseRuleStore}
 */
cvox.MathStore = function() {
  goog.base(this);

  /**
   * @override
   */
  this.dynamicCstrAttribs = [
    cvox.SpeechRule.DynamicCstrAttrib.DOMAIN,
    cvox.SpeechRule.DynamicCstrAttrib.STYLE
    ];

  /**
   * @override
   */
  this.defaultTtsProps = [cvox.AbstractTts.PITCH];
};
goog.inherits(cvox.MathStore, cvox.BaseRuleStore);

/** This adds domain to dynamic constraint annotation. */
cvox.SpeechRule.DynamicCstrAttrib.DOMAIN = 'domain';


/**
 * @override
 */
cvox.MathStore.prototype.defineRule = function(
    name, dynamic, action, query, cstr) {
  var dynamicCstr = this.parseDynamicConstraint(dynamic);
  var cstrList = Array.prototype.slice.call(arguments, 4);
  // We can not use goog.base due to variable number of constraint arguments.
  var rule = cvox.MathStore.superClass_.defineRule.apply(
      this, [name, dynamicCstr[cvox.SpeechRule.DynamicCstrAttrib.STYLE],
             action, query].concat(cstrList));
  // In the superclass the dynamic constraint only contains style annotations.
  // We now set the proper dynamic constraint that contains in addition a
  // a domain attribute/value pair.
  rule.dynamicCstr = dynamicCstr;
  this.removeDuplicates(rule);
  return rule;
};


/**
 * Parses the dynamic constraint for math rules, consisting of a domain and
 * style information, given as 'domain.style'.
 * @param {string} cstr A string representation of the dynamic constraint.
 * @return {!cvox.SpeechRule.DynamicCstr} The dynamic constraint.
 */
cvox.MathStore.prototype.parseDynamicConstraint = function(cstr) {
  var domainStyle = cstr.split('.');
  if (!domainStyle[0] || !domainStyle[1]) {
    throw new cvox.SpeechRule.OutputError('Invalid domain assignment:' + cstr);
  }
  return cvox.MathStore.createDynamicConstraint(domainStyle[0], domainStyle[1]);
};


/**
 * Creates a dynamic constraint annotation for math rules from domain and style
 * values.
 * @param {string} domain Domain annotation.
 * @param {string} style Style annotation.
 * @return {!cvox.SpeechRule.DynamicCstr}
 */
cvox.MathStore.createDynamicConstraint = function(domain, style) {
  var dynamicCstr = {};
  dynamicCstr[cvox.SpeechRule.DynamicCstrAttrib.DOMAIN] = domain;
  dynamicCstr[cvox.SpeechRule.DynamicCstrAttrib.STYLE] = style;
  return dynamicCstr;
};


/**
 * Adds an alias for an existing rule.
 * @param {string} name The name of the rule.
 * @param {string} dynamic A math domain and style assignment.
 * @param {string} query Precondition query of the rule.
 * @param {...string} cstr Additional static precondition constraints.
 */
cvox.MathStore.prototype.defineUniqueRuleAlias = function(
    name, dynamic, query, cstr) {
  var dynamicCstr = this.parseDynamicConstraint(dynamic);
  var rule = this.findRule(
    goog.bind(
      function(rule) {
        return rule.name == name &&
          this.testDynamicConstraints(dynamicCstr, rule);},
      this));
  if (!rule) {
    throw new cvox.SpeechRule.OutputError(
        'Rule named ' + name + ' with style ' + dynamic + ' does not exist.');
  }
  this.addAlias_(rule, query, Array.prototype.slice.call(arguments, 3));
};


/**
 * Adds an alias for an existing rule.
 * @param {string} name The name of the rule.
 * @param {string} query Precondition query of the rule.
 * @param {...string} cstr Additional static precondition constraints.
 */
cvox.MathStore.prototype.defineRuleAlias = function(name, query, cstr) {
  var rule = this.findRule(function(rule) {
    return rule.name == name;});
  if (!rule) {
    throw new cvox.SpeechRule.OutputError(
      'Rule with named ' + name + ' does not exist.');
    }
  this.addAlias_(rule, query, Array.prototype.slice.call(arguments, 2));
};


/**
 * Adds an alias for an existing rule.
 * @param {string} name The name of the rule.
 * @param {string} query Precondition query of the rule.
 * @param {...string} cstr Additional static precondition constraints.
 */
cvox.MathStore.prototype.defineRulesAlias = function(name, query, cstr) {
  var rules = this.findAllRules(function(rule) {return rule.name == name;});
  if (rules.length == 0) {
    throw new cvox.SpeechRule.OutputError(
        'Rule with name ' + name + ' does not exist.');
  }
  var cstrList = Array.prototype.slice.call(arguments, 2);
  rules.forEach(goog.bind(
                  function(rule) {
                    this.addAlias_(rule, query, cstrList);
                  },
                  this));
};


/**
 * Adds a new speech rule as alias of the given rule.
 * @param {cvox.SpeechRule} rule The existing rule.
 * @param {string} query Precondition query of the rule.
 * @param {Array<string>} cstrList List of additional constraints.
 * @private
 */
cvox.MathStore.prototype.addAlias_ = function(rule, query, cstrList) {
  var prec = new cvox.SpeechRule.Precondition(query, cstrList);
  var newRule = new cvox.SpeechRule(
      rule.name, rule.dynamicCstr, prec, rule.action);
  newRule.name = rule.name;
  this.addRule(newRule);
};


// Evaluator
/**
 * @override
 */
cvox.MathStore.prototype.evaluateDefault = function(node) {
  return this.evaluateString_(node.textContent);
};


/**
 * Evaluates a single string of a math expressions. The method splits the given
 * string into components such as single characters, function names or words,
 * numbers, etc. and creates the appropriate navigation descriptions.
 * @param {string} str A string.
 * @return {!Array<cvox.NavDescription>} Messages for the math expression.
 * @private
 */
cvox.MathStore.prototype.evaluateString_ = function(str) {
  var descs = new Array();
  if (str.match(/^\s+$/)) {
    // Nothing but whitespace: Ignore.
    return descs;
  }
  var split = cvox.MathStore.removeEmpty_(str.replace(/\s/g, ' ').split(' '));
  for (var i = 0, s; s = split[i]; i++) {
    if (s.length == 1) {
      descs.push(this.evaluate_(s));
    } else if (s.match(/^[a-zA-Z]+$/)) {
      descs.push(this.evaluate_(s));
    } else {
      // Break up string even further wrt. symbols vs alphanum substrings.
      var rest = s;
      var count = 0;
      while (rest) {
        var num = rest.match(/^\d+/);
        var alpha = rest.match(/^[a-zA-Z]+/);
        if (num) {
          descs.push(this.evaluate_(num[0]));
          rest = rest.substring(num[0].length);
        } else if (alpha) {
          descs.push(this.evaluate_(alpha[0]));
          rest = rest.substring(alpha[0].length);
        } else {
          // Dealing with surrogate pairs.
          var chr = rest[0];
          var code = chr.charCodeAt(0);
          if (0xD800 <= code && code <= 0xDBFF &&
              rest.length > 1 && !isNaN(rest.charCodeAt(1))) {
            descs.push(this.evaluate_(rest.slice(0, 2)));
            rest = rest.substring(2);
          } else {
            descs.push(this.evaluate_(rest[0]));
            rest = rest.substring(1);
            }
        }
      }
    }
  }
  return descs;
};


/**
 * Creates a new Navigation Description for a math expression that be used by
 * the background tts.
 * @param {string} text to be translated.
 * @return {cvox.NavDescription} Navigation description for the
 *     math expression.
 * @private
 */
cvox.MathStore.prototype.evaluate_ = function(text) {
  if (cvox.ChromeVox.host['mathMap']) {
    // VS: Change this for android!
    return cvox.ChromeVox.host['mathMap'].evaluate(
        text,
        cvox.TraverseMath.getInstance().domain,
        cvox.TraverseMath.getInstance().style);
  }
  return new cvox.NavMathDescription(
      {'text': text,
       'domain': cvox.TraverseMath.getInstance().domain,
       'style': cvox.TraverseMath.getInstance().style
      });
};


/**
 * Removes all empty strings from an array of strings.
 * @param {Array<string>} strs An array of strings.
 * @return {Array<string>} The cleaned array.
 * @private
 */
cvox.MathStore.removeEmpty_ = function(strs) {
  return strs.filter(function(str) {return str;});
};

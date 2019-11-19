// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface definition of a speech rule.
 *
 * A speech rule is a data structure along with supporting methods that
 * stipulates how to transform a tree structure such as XML, a browser DOM, or
 * HTML into a format (usually strings) suitable for rendering by a
 * text-to-speech engine.
 *
 * Speech rules consists of a variable number of speech rule components. Each
 * component describes how to construct a single utterance. Text-to-speech
 * renders the components in order.
 */

goog.provide('cvox.SpeechRule');
goog.provide('cvox.SpeechRule.Action');
goog.provide('cvox.SpeechRule.Component');
goog.provide('cvox.SpeechRule.DynamicCstr');
goog.provide('cvox.SpeechRule.Precondition');
goog.provide('cvox.SpeechRule.Type');


/**
 * Creates a speech rule with precondition, actions and admin information.
 * @constructor
 * @param {string} name The name of the rule.
 * @param {cvox.SpeechRule.DynamicCstr} dynamic Dynamic constraint annotations
 *     of the rule.
 * @param {cvox.SpeechRule.Precondition} prec Precondition of the rule.
 * @param {cvox.SpeechRule.Action} action Action of the speech rule.
 */
cvox.SpeechRule = function(name, dynamic, prec, action) {
  /** @type {string} */
  this.name = name;
  /** @type {cvox.SpeechRule.DynamicCstr} */
  this.dynamicCstr = dynamic;
  /** @type {cvox.SpeechRule.Precondition} */
  this.precondition = prec;
  /** @type {cvox.SpeechRule.Action} */
  this.action = action;
};


/**
 *
 * @override
 */
cvox.SpeechRule.prototype.toString = function() {
  var cstrStrings = [];
  for (var key in this.dynamicCstr) {
    cstrStrings.push(this.dynamicCstr[key]);
  }
  return this.name + ' | ' + cstrStrings.join('.') + ' | ' +
    this.precondition.toString() + ' ==> ' +
    this.action.toString();
};


/**
 * Mapping for types of speech rule components.
 * @enum {string}
 */
cvox.SpeechRule.Type = {
  NODE: 'NODE',
  MULTI: 'MULTI',
  TEXT: 'TEXT',
  PERSONALITY: 'PERSONALITY'
};


/**
 * Maps a string to a valid speech rule type.
 * @param {string} str Input string.
 * @return {cvox.SpeechRule.Type}
 */
cvox.SpeechRule.Type.fromString = function(str) {
  switch (str) {
    case '[n]': return cvox.SpeechRule.Type.NODE;
    case '[m]': return cvox.SpeechRule.Type.MULTI;
    case '[t]': return cvox.SpeechRule.Type.TEXT;
    case '[p]': return cvox.SpeechRule.Type.PERSONALITY;
    default: throw 'Parse error: ' + str;
  }
};


/**
 * Maps a speech rule type to a human-readable string.
 * @param {cvox.SpeechRule.Type} speechType
 * @return {string} Output string.
 */
cvox.SpeechRule.Type.toString = function(speechType) {
  switch (speechType) {
    case cvox.SpeechRule.Type.NODE: return '[n]';
    case cvox.SpeechRule.Type.MULTI: return '[m]';
    case cvox.SpeechRule.Type.TEXT: return '[t]';
    case cvox.SpeechRule.Type.PERSONALITY: return '[p]';
    default: throw 'Unknown type error: ' + speechType;
  }
};


/**
 * Defines a component within a speech rule.
 * @param {{type: cvox.SpeechRule.Type, content: string}} kwargs The input
 * component in JSON format.
 * @constructor
 */
cvox.SpeechRule.Component = function(kwargs) {
  /** @type {cvox.SpeechRule.Type} */
  this.type = kwargs.type;

  /** @type {string} */
  this.content = kwargs.content;
};


/**
 * Parses a valid string representation of a speech component into a Component
 * object.
 * @param {string} input The input string.
 * @return {cvox.SpeechRule.Component} The resulting component.
 */
cvox.SpeechRule.Component.fromString = function(input) {
  // The output JSON.
  var output = {};

  // Parse the type.
  output.type = cvox.SpeechRule.Type.fromString(input.substring(0, 3));

  // Prep the rest of the parsing.
  var rest = input.slice(3).trimLeft();
  if (!rest) {
    throw new cvox.SpeechRule.OutputError('Missing content.');
  }

  switch (output.type) {
    case cvox.SpeechRule.Type.TEXT:
      if (rest[0] == '"') {
        var quotedString = cvox.SpeechRule.splitString_(rest, '\\(')[0].trim();
        if (quotedString.slice(-1) != '"') {
          throw new cvox.SpeechRule.OutputError('Invalid string syntax.');
        }
        output.content = quotedString;
        rest = rest.slice(quotedString.length).trim();
        if (rest.indexOf('(') == -1) {
          rest = '';
        }
        // This break is conditional. If the content is not an explicit string,
        // it can be treated like node and multi type.
        break;
      }
    case cvox.SpeechRule.Type.NODE:
    case cvox.SpeechRule.Type.MULTI:
      var bracket = rest.indexOf(' (');
      if (bracket == -1) {
        output.content = rest.trim();
        rest = '';
        break;
      }
      output.content = rest.substring(0, bracket).trim();
      rest = rest.slice(bracket).trimLeft();
    break;
  }
  output = new cvox.SpeechRule.Component(output);
  if (rest) {
    output.addAttributes(rest);
  }
  return output;
};


/**
 * @override
 */
cvox.SpeechRule.Component.prototype.toString = function() {
  var strs = '';
  strs += cvox.SpeechRule.Type.toString(this.type);
  strs += this.content ? ' ' + this.content : '';
  var attribs = this.getAttributes();
  if (attribs.length > 0) {
    strs += ' (' + attribs.join(', ') + ')';
  }
  return strs;
};


/**
 * Adds a single attribute to the component.
 * @param {string} attr String representation of an attribute.
 */
cvox.SpeechRule.Component.prototype.addAttribute = function(attr) {
  var colon = attr.indexOf(':');
  if (colon == -1) {
    this[attr.trim()] = 'true';
  } else {
    this[attr.substring(0, colon).trim()] = attr.slice(colon + 1).trim();
  }
};


/**
 * Adds a list of attributes to the component.
 * @param {string} attrs String representation of attribute list.
 */
cvox.SpeechRule.Component.prototype.addAttributes = function(attrs) {
  if (attrs[0] != '(' || attrs.slice(-1) != ')') {
    throw new cvox.SpeechRule.OutputError(
        'Invalid attribute expression: ' + attrs);
  }
  var attribs = cvox.SpeechRule.splitString_(attrs.slice(1, -1), ',');
  for (var i = 0; i < attribs.length; i++) {
    this.addAttribute(attribs[i]);
  }
};


/**
 * Transforms the attributes of an object into a list of strings.
 * @return {Array<string>} List of translated attribute:value strings.
 */
cvox.SpeechRule.Component.prototype.getAttributes = function() {
  var attribs = [];
  for (var key in this) {
    if (key != 'content' && key != 'type' && typeof(this[key]) != 'function') {
      attribs.push(key + ':' + this[key]);
    }
  }
  return attribs;
};


/**
 * A speech rule is a collection of speech components.
 * @param {Array<cvox.SpeechRule.Component>} components The input rule.
 * @constructor
 */
cvox.SpeechRule.Action = function(components) {
  /** @type {Array<cvox.SpeechRule.Component>} */
  this.components = components;
};


/**
 * Parses an input string into a speech rule class object.
 * @param {string} input The input string.
 * @return {cvox.SpeechRule.Action} The resulting object.
 */
cvox.SpeechRule.Action.fromString = function(input) {
  var comps = cvox.SpeechRule.splitString_(input, ';')
      .filter(function(x) {return x.match(/\S/);})
      .map(function(x) {return x.trim();});
  var newComps = [];
  for (var i = 0; i < comps.length; i++) {
    var comp = cvox.SpeechRule.Component.fromString(comps[i]);
    if (comp) {
      newComps.push(comp);
    }
  }
return new cvox.SpeechRule.Action(newComps);
};


/**
 * @override
 */
cvox.SpeechRule.Action.prototype.toString = function() {
  var comps = this.components.map(function(c) { return c.toString(); });
  return comps.join('; ');
};


// TODO (sorge) Separatation of xpath expressions and custom functions.
// Also test validity of xpath expressions.
/**
 * Constructs a valid precondition for a speech rule.
 * @param {string} query A node selector function or xpath expression.
 * @param {Array<string>=} opt_constraints A list of constraint functions.
 * @constructor
 */
cvox.SpeechRule.Precondition = function(query, opt_constraints) {
  /** @type {string} */
  this.query = query;

  /** @type {!Array<string>} */
  this.constraints = opt_constraints || [];
};


/**
 * @override
 */
cvox.SpeechRule.Precondition.prototype.toString = function() {
  var constrs = this.constraints.join(', ');
  return this.query + ', ' + constrs;
};


/**
 * Split a string wrt. a given separator symbol while not splitting inside of a
 * double quoted string. For example, splitting
 * '[t] "matrix; 3 by 3"; [n] ./*[1]' with separators ';' would yield
 * ['[t] "matrix; 3 by 3"', ' [n] ./*[1]'].
 * @param {string} str String to be split.
 * @param {string} sep Separator symbol.
 * @return {Array<string>} A list of single component strings.
 * @private
 */
cvox.SpeechRule.splitString_ = function(str, sep) {
  var strList = [];
  var prefix = '';

  while (str != '') {
    var sepPos = str.search(sep);
    if (sepPos == -1) {
      if ((str.match(/"/g) || []).length % 2 != 0) {
        throw new cvox.SpeechRule.OutputError(
            'Invalid string in expression: ' + str);
      }
      strList.push(prefix + str);
      prefix = '';
      str = '';
    } else if (
        (str.substring(0, sepPos).match(/"/g) || []).length % 2 == 0) {
      strList.push(prefix + str.substring(0, sepPos));
      prefix = '';
      str = str.substring(sepPos + 1);
    } else {
      var nextQuot = str.substring(sepPos).search('"');
      if (nextQuot == -1) {
        throw new cvox.SpeechRule.OutputError(
            'Invalid string in expression: ' + str);
      } else {
        prefix = prefix + str.substring(0, sepPos + nextQuot + 1);
        str = str.substring(sepPos + nextQuot + 1);
      }
    }
  }
  if (prefix) {
    strList.push(prefix);
  }
  return strList;
};


/**
 * Attributes for dynamic constraints.
 * We define one default attribute as style. Speech rule stores can add other
 * attributes later.
 * @enum {string}
 */
cvox.SpeechRule.DynamicCstrAttrib =
{
  STYLE: 'style'
};


/**
 * Dynamic constraints are a means to specialize rules that can be changed
 * dynamically by the user, for example by choosing different styles, etc.
 * @typedef {!Object<cvox.SpeechRule.DynamicCstrAttrib, string>}
 */
cvox.SpeechRule.DynamicCstr;


/**
 * Error object for signaling parsing errors.
 * @param {string} msg The error message.
 * @constructor
 * @extends {Error}
 */
cvox.SpeechRule.OutputError = function(msg) {
  this.name = 'RuleError';
  this.message = msg || '';
};
goog.inherits(cvox.SpeechRule.OutputError, Error);

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implementation of the speech rule engine.
 *
 * The speech rule engine chooses and applies speech rules. Rules are chosen
 * from a set of rule stores wrt. their applicability to a node in a particular
 * markup type such as MathML or HTML. Rules are dispatched either by
 * recursively computing new nodes and applicable rules or, if no further rule
 * is applicable to a current node, by computing a speech object in the form of
 * an array of navigation descriptions.
 *
 * Consequently the rule engine is parameterisable wrt. rule stores and
 * evaluator function.
 */

goog.provide('cvox.SpeechRuleEngine');

goog.require('cvox.BaseRuleStore');
goog.require('cvox.NavDescription');
goog.require('cvox.NavMathDescription');
goog.require('cvox.SpeechRule');


/**
 * @constructor
 */
cvox.SpeechRuleEngine = function() {
  /**
   * The currently active speech rule store.
   * @type {cvox.BaseRuleStore}
   * @private
   */
  this.activeStore_ = null;

  /**
   * Dynamic constraint annotation.
   * @type {!cvox.SpeechRule.DynamicCstr}
   */
  this.dynamicCstr = {};
  this.dynamicCstr[cvox.SpeechRule.DynamicCstrAttrib.STYLE] = 'short';
};
goog.addSingletonGetter(cvox.SpeechRuleEngine);


/**
 * Parameterizes the speech rule engine.
 * @param {cvox.BaseRuleStore} store A speech rule store.
 */
cvox.SpeechRuleEngine.prototype.parameterize = function(store) {
  try {
    store.initialize();
  } catch (err) {
    if (err.name == 'StoreError') {
      console.log('Store Error:', err.message);
    }
    else {
      throw err;
    }
  }
  this.activeStore_ = store;
};


/**
 * Parameterizes the dynamic constraint annotation for the speech rule
 * engine. This is a separate function as this can be done interactively, while
 * a particular speech rule store is active.
 * @param {cvox.SpeechRule.DynamicCstr} dynamic The new dynamic constraint.
 */
cvox.SpeechRuleEngine.prototype.setDynamicConstraint = function(dynamic) {
  if (dynamic) {
    this.dynamicCstr = dynamic;
  }
};


/**
 * Constructs a string from the node and the given expression.
 * @param {!Node} node The initial node.
 * @param {string} expr An Xpath expression string, a name of a custom
 *     function or a string.
 * @return {string} The result of applying expression to node.
 */
cvox.SpeechRuleEngine.prototype.constructString = function(node, expr) {
  if (!expr) {
    return '';
  }
  if (expr.charAt(0) == '"') {
    return expr.slice(1, -1);
  }
  var func = this.activeStore_.customStrings.lookup(expr);
  if (func) {
    // We always return the result of the custom function, in case it
    // deliberately computes the empty string!
    return func(node);
  }
  // Finally we assume expr to be an xpath expression and calculate a string
  // value from the node.
  return cvox.XpathUtil.evaluateString(expr, node);
};


// Dispatch functionality.
/**
 * Computes a speech object for a given node. Returns the empty list if
 * no node is given.
 * @param {Node} node The node to be evaluated.
 * @return {!Array<cvox.NavDescription>} A list of navigation descriptions for
 *   that node.
 */
cvox.SpeechRuleEngine.prototype.evaluateNode = function(node) {
  if (!node) {
    return [];
  }
  return this.evaluateTree_(node);
};


/**
 * Applies rules recursively to compute the final speech object.
 * @param {!Node} node Node to apply the speech rule to.
 * @return {!Array<cvox.NavDescription>} A list of Navigation descriptions.
 * @private
 */
cvox.SpeechRuleEngine.prototype.evaluateTree_ = function(node) {
  var rule = this.activeStore_.lookupRule(node, this.dynamicCstr);
  if (!rule) {
    return this.activeStore_.evaluateDefault(node);
  }
  var components = rule.action.components;
  var result = [];
  for (var i = 0, component; component = components[i]; i++) {
    var navs = [];
    var content = component['content'] || '';
    switch (component.type) {
      case cvox.SpeechRule.Type.NODE:
        var selected = this.activeStore_.applyQuery(node, content);
        if (selected) {
          navs = this.evaluateTree_(selected);
        }
        break;
      case cvox.SpeechRule.Type.MULTI:
        selected = this.activeStore_.applySelector(node, content);
        if (selected.length > 0) {
          navs = this.evaluateNodeList_(
              selected,
              component['sepFunc'],
              this.constructString(node, component['separator']),
              component['ctxtFunc'],
              this.constructString(node, component['context']));
        }
        break;
      case cvox.SpeechRule.Type.TEXT:
        selected = this.constructString(node, content);
        if (selected) {
          navs = [new cvox.NavDescription({text: selected})];
        }
        break;
      case cvox.SpeechRule.Type.PERSONALITY:
      default:
        navs = [new cvox.NavDescription({text: content})];
    }
    // Adding overall context if it exists.
    if (navs[0] && component['context'] &&
        component.type != cvox.SpeechRule.Type.MULTI) {
      navs[0]['context'] =
          this.constructString(node, component['context']) +
              (navs[0]['context'] || '');
    }
    // Adding personality to the nav descriptions.
    result = result.concat(this.addPersonality_(navs, component));
  }
  return result;
};


/**
 * Evaluates a list of nodes into a list of navigation descriptions.
 * @param {!Array<Node>} nodes Array of nodes.
 * @param {string} sepFunc Name of a function used to compute a separator
 *     between every element.
 * @param {string} separator A string that is used as argument to the sepFunc or
 *     interspersed directly between each node if sepFunc is not supplied.
 * @param {string} ctxtFunc Name of a function applied to compute the context
 *     for every element in the list.
 * @param {string} context Additional context string that is given to the
 *     ctxtFunc function or used directly if ctxtFunc is not supplied.
 * @return {Array<cvox.NavDescription>} A list of Navigation descriptions.
 * @private
 */
cvox.SpeechRuleEngine.prototype.evaluateNodeList_ = function(
    nodes, sepFunc, separator, ctxtFunc, context) {
  if (nodes == []) {
    return [];
  }
  var sep = separator || '';
  var cont = context || '';
  var cFunc = this.activeStore_.contextFunctions.lookup(ctxtFunc);
  var ctxtClosure = cFunc ? cFunc(nodes, cont) : function() {return cont;};
  var sFunc = this.activeStore_.contextFunctions.lookup(sepFunc);
  var sepClosure = sFunc ? sFunc(nodes, sep) : function() {return sep;};
  var result = [];
  for (var i = 0, node; node = nodes[i]; i++) {
    var navs = this.evaluateTree_(node);
    if (navs.length > 0) {
      navs[0]['context'] = ctxtClosure() + (navs[0]['context'] || '');
      result = result.concat(navs);
      if (i < nodes.length - 1) {
        var text = sepClosure();
        if (text) {
          result.push(new cvox.NavDescription({text: text}));
        }
      }
    }
  }
  return result;
};


/**
 * Maps properties in speech rules to personality properties.
 * @type {{pitch : string,
 *         rate: string,
 *         volume: string,
 *         pause: string}}
 * @const
 */
cvox.SpeechRuleEngine.propMap = {'pitch': cvox.AbstractTts.RELATIVE_PITCH,
                                 'rate': cvox.AbstractTts.RELATIVE_RATE,
                                 'volume': cvox.AbstractTts.RELATIVE_VOLUME,
                                 'pause': cvox.AbstractTts.PAUSE
                                };


/**
 * Adds personality to every Navigation Descriptions in input list.
 * @param {Array<cvox.NavDescription>} navs A list of Navigation descriptions.
 * @param {Object} props Property dictionary.
 * TODO (sorge) Fully specify, when we have finalised the speech rule
 * format.
 * @return {Array<cvox.NavDescription>} The modified array.
 * @private
 */
cvox.SpeechRuleEngine.prototype.addPersonality_ = function(navs, props) {
  var personality = {};
  for (var key in cvox.SpeechRuleEngine.propMap) {
    var value = parseFloat(props[key]);
    if (!isNaN(value)) {
      personality[cvox.SpeechRuleEngine.propMap[key]] = value;
    }
  }
  navs.forEach(goog.bind(function(nav) {
    this.addRelativePersonality_(nav, personality);
    this.resetPersonality_(nav);
  }, this));
  return navs;
};


/**
 * Adds relative personality entries to the personality of a Navigation
 * Description.
 * @param {cvox.NavDescription|cvox.NavMathDescription} nav Nav Description.
 * @param {!Object} personality Dictionary with relative personality entries.
 * @return {cvox.NavDescription|cvox.NavMathDescription} Updated description.
 * @private
 */
cvox.SpeechRuleEngine.prototype.addRelativePersonality_ = function(
    nav, personality) {
  if (!nav['personality']) {
    nav['personality'] = personality;
    return nav;
  }
  var navPersonality = nav['personality'];
  for (var p in personality) {
    // Although values could exceed boundaries, they will be limited to the
    // correct interval via the call to
    // cvox.AbstractTts.prototype.mergeProperties in
    // cvox.TtsBackground.prototype.speak
    if (navPersonality[p] && typeof(navPersonality[p]) == 'number') {
      navPersonality[p] = navPersonality[p] + personality[p];
    } else {
      navPersonality[p] = personality[p];
    }
  }
  return nav;
};


/**
 * Resets personalities to default values if necessary.
 * @param {cvox.NavDescription|cvox.NavMathDescription} nav Nav Description.
 * @private
 */
cvox.SpeechRuleEngine.prototype.resetPersonality_ = function(nav) {
  if (this.activeStore_.defaultTtsProps) {
    for (var i = 0, prop; prop = this.activeStore_.defaultTtsProps[i]; i++) {
      nav.personality[prop] = cvox.ChromeVox.tts.getDefaultProperty(prop);
    }
  }
};


/**
 * Flag for the debug mode of the speech rule engine.
 * @type {boolean}
 */
cvox.SpeechRuleEngine.debugMode = false;


/**
 * Give debug output.
 * @param {...*} output Rest elements of debug output.
 */
cvox.SpeechRuleEngine.outputDebug = function(output) {
  if (cvox.SpeechRuleEngine.debugMode) {
    var outputList = Array.prototype.slice.call(arguments, 0);
    console.log.apply(console,
                      ['Speech Rule Engine Debugger:'].concat(outputList));
  }
};


/**
 * Prints the list of all current rules in ChromeVox to the console.
 * @return {string} A textual representation of all rules in the speech rule
 *     engine.
 */
cvox.SpeechRuleEngine.prototype.toString = function() {
  var allRules = this.activeStore_.findAllRules(function(x) {return true;});
  return allRules.map(function(rule) {return rule.toString();}).
    join('\n');
};


/**
 * Test the precondition of a speech rule in debugging mode.
 * @param {cvox.SpeechRule} rule A speech rule.
 * @param {!Node} node DOM node to test applicability of the rule.
 */
cvox.SpeechRuleEngine.debugSpeechRule = function(rule, node) {
  var store = cvox.SpeechRuleEngine.getInstance().activeStore_;
  if (store) {
    var prec = rule.precondition;
    cvox.SpeechRuleEngine.outputDebug(
        prec.query, store.applyQuery(node, prec.query));
    prec.constraints.forEach(
        function(cstr) {
          cvox.SpeechRuleEngine.outputDebug(
              cstr, store.applyConstraint(node, cstr));});
  }
};


/**
 * Test the precondition of a speech rule in debugging mode.
 * @param {string} name Rule to debug.
 * @param {!Node} node DOM node to test applicability of the rule.
 */
cvox.SpeechRuleEngine.debugNamedSpeechRule = function(name, node) {
  var store = cvox.SpeechRuleEngine.getInstance().activeStore_;
  var allRules = store.findAllRules(
    function(rule) {return rule.name == name;});
  for (var i = 0, rule; rule = allRules[i]; i++) {
    cvox.SpeechRuleEngine.outputDebug('Rule', name, 'number', i);
    cvox.SpeechRuleEngine.debugSpeechRule(rule, node);
  }
};

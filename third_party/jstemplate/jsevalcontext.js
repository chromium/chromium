// Copyright 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
/**
 * Author: Steffen Meschkat <mesch@google.com>
 *
 * @fileoverview This class is used to evaluate expressions in a local
 * context. Used by JstProcessor.
 */


/**
 * Names of special variables defined by the jstemplate evaluation
 * context. These can be used in js expression in jstemplate
 * attributes.
 */
var VAR_index = '$index';
var VAR_count = '$count';
var VAR_this = '$this';
var VAR_context = '$context';
var VAR_top = '$top';


/**
 * The name of the global variable which holds the value to be returned if
 * context evaluation results in an error. 
 * Use JsEvalContext.setGlobal(GLOB_default, value) to set this.
 */
var GLOB_default = '$default';


/**
 * Un-inlined literals, to avoid object creation in IE6. TODO(mesch):
 * So far, these are only used here, but we could use them thoughout
 * the code and thus move them to constants.js.
 */
var CHAR_colon = ':';
var REGEXP_semicolon = /\s*;\s*/;


/**
 * See constructor_()
 * @param {Object|null=} opt_data
 * @param {Object=} opt_parent
 * @constructor
 */
function JsEvalContext(opt_data, opt_parent) {
  this.constructor_.apply(this, arguments);
}

/**
 * Context for processing a jstemplate. The context contains a context
 * object, whose properties can be referred to in jstemplate
 * expressions, and it holds the locally defined variables.
 *
 * @param {Object|null} opt_data The context object. Null if no context.
 *
 * @param {Object} opt_parent The parent context, from which local
 * variables are inherited. Normally the context object of the parent
 * context is the object whose property the parent object is. Null for the
 * context of the root object.
 * @private
 */
JsEvalContext.prototype.constructor_ = function(opt_data, opt_parent) {
  var me = this;

  if (!me.vars_) {
    /**
     * The context for variable definitions in which the jstemplate
     * expressions are evaluated. Other than for the local context,
     * which replaces the parent context, variable definitions of the
     * parent are inherited. The special variable $this points to data_.
     *
     * If this instance is recycled from the cache, then the property is
     * already initialized.
     *
     * @type {Object}
     */
    me.vars_ = {};
  }
  if (opt_parent) {
    // If there is a parent node, inherit local variables from the
    // parent.
    copyProperties(me.vars_, opt_parent.vars_);
  } else {
    // If a root node, inherit global symbols. Since every parent
    // chain has a root with no parent, global variables will be
    // present in the case above too. This means that globals can be
    // overridden by locals, as it should be.
    copyProperties(me.vars_, JsEvalContext.globals_);
  }

  /**
   * The current context object is assigned to the special variable
   * $this so it is possible to use it in expressions.
   * @type {Object}
   */
  me.vars_[VAR_this] = opt_data;

  /**
   * The entire context structure is exposed as a variable so it can be
   * passed to javascript invocations through jseval.
   */
  me.vars_[VAR_context] = me;

  /**
   * The local context of the input data in which the jstemplate
   * expressions are evaluated. Notice that this is usually an Object,
   * but it can also be a scalar value (and then still the expression
   * $this can be used to refer to it). Notice this can even be value,
   * undefined or null. Hence, we have to protect jsexec() from using
   * undefined or null, yet we want $this to reflect the true value of
   * the current context. Thus we assign the original value to $this,
   * above, but for the expression context we replace null and
   * undefined by the empty string.
   *
   * @type {*}
   */
  me.data_ = getDefaultObject(opt_data, STRING_empty);

  if (!opt_parent) {
    // If this is a top-level context, create a variable reference to the data
    // to allow for  accessing top-level properties of the original context
    // data from child contexts.
    me.vars_[VAR_top] = me.data_;
  }
};


/**
 * A map of globally defined symbols. Every instance of JsExprContext
 * inherits them in its vars_.
 * @type Object
 */
JsEvalContext.globals_ = {}


/**
 * Sets a global symbol. It will be available like a variable in every
 * JsEvalContext instance. This is intended mainly to register
 * immutable global objects, such as functions, at load time, and not
 * to add global data at runtime. I.e. the same objections as to
 * global variables in general apply also here. (Hence the name
 * "global", and not "global var".)
 * @param {string} name
 * @param {Object|null} value
 */
JsEvalContext.setGlobal = function(name, value) {
  JsEvalContext.globals_[name] = value;
};


/**
 * Set the default value to be returned if context evaluation results in an 
 * error. (This can occur if a non-existent value was requested). 
 */
JsEvalContext.setGlobal(GLOB_default, null);


/**
 * A cache to reuse JsEvalContext instances. (IE6 perf)
 *
 * @type Array.<JsEvalContext>
 */
JsEvalContext.recycledInstances_ = [];


/**
 * A factory to create a JsEvalContext instance, possibly reusing
 * one from recycledInstances_. (IE6 perf)
 *
 * @param {Object} opt_data
 * @param {JsEvalContext} opt_parent
 * @return {JsEvalContext}
 */
JsEvalContext.create = function(opt_data, opt_parent) {
  if (jsLength(JsEvalContext.recycledInstances_) > 0) {
    var instance = JsEvalContext.recycledInstances_.pop();
    JsEvalContext.call(instance, opt_data, opt_parent);
    return instance;
  } else {
    return new JsEvalContext(opt_data, opt_parent);
  }
};


/**
 * Recycle a used JsEvalContext instance, so we can avoid creating one
 * the next time we need one. (IE6 perf)
 *
 * @param {JsEvalContext} instance
 */
JsEvalContext.recycle = function(instance) {
  for (var i in instance.vars_) {
    // NOTE(mesch): We avoid object creation here. (IE6 perf)
    delete instance.vars_[i];
  }
  instance.data_ = null;
  JsEvalContext.recycledInstances_.push(instance);
};


/**
 * Executes a function created using jsEvalToFunction() in the context
 * of vars, data, and template.
 *
 * @param {Function} exprFunction A javascript function created from
 * a jstemplate attribute value.
 *
 * @param {Element} template DOM node of the template.
 *
 * @return {Object|null} The value of the expression from which
 * exprFunction was created in the current js expression context and
 * the context of template.
 */
JsEvalContext.prototype.jsexec = function(exprFunction, template) {
  try {
    return exprFunction.call(template, this.vars_, this.data_);
  } catch (e) {
    log('jsexec EXCEPTION: ' + e + ' at ' + template +
        ' with ' + exprFunction);
    return JsEvalContext.globals_[GLOB_default];
  }
};


/**
 * Clones the current context for a new context object. The cloned
 * context has the data object as its context object and the current
 * context as its parent context. It also sets the $index variable to
 * the given value. This value usually is the position of the data
 * object in a list for which a template is instantiated multiply.
 *
 * @param {Object} data The new context object.
 *
 * @param {number|string} index Position of the new context when multiply
 * instantiated. (See implementation of jstSelect().)
 * 
 * @param {number} count The total number of contexts that were multiply
 * instantiated. (See implementation of jstSelect().)
 *
 * @return {JsEvalContext}
 */
JsEvalContext.prototype.clone = function(data, index, count) {
  var ret = JsEvalContext.create(data, this);
  ret.setVariable(VAR_index, index);
  ret.setVariable(VAR_count, count);
  return ret;
};


/**
 * Binds a local variable to the given value. If set from jstemplate
 * jsvalue expressions, variable names must start with $, but in the
 * API they only have to be valid javascript identifier.
 *
 * @param {string} name
 * @param {*} value
 */
JsEvalContext.prototype.setVariable = function(name, value) {
  this.vars_[name] = value;
};


/**
 * Returns the value bound to the local variable of the given name, or
 * undefined if it wasn't set. There is no way to distinguish a
 * variable that wasn't set from a variable that was set to
 * undefined. Used mostly for testing.
 *
 * @param {string} name
 *
 * @return {*} value
 */
JsEvalContext.prototype.getVariable = function(name) {
  return this.vars_[name];
};


/**
 * Evaluates a string expression within the scope of this context
 * and returns the result.
 *
 * @param {string} expr A javascript expression
 * @param {Element} opt_template An optional node to serve as "this"
 *
 * @return {Object?} value
 */
JsEvalContext.prototype.evalExpression = function(expr, opt_template) {
  var exprFunction = jsEvalToFunction(expr);
  return this.jsexec(exprFunction, opt_template);
};


/**
 * This is used to create TrustedScript.
 *
 * @type {!TrustedTypePolicy}
 */
let unsanitizedPolicy;
if (window.trustedTypes) {
  // This is relatively safe because attribute's values can
  // only reach here with `JsEvalContext` bootstrap. And even
  // if opaqueScript calls dangerous sinks (e.g. innerHTML),
  // it'll still be subject to type check with Trusted Types.
  // This could be exploited if bootstrap is called with an
  // event which can be triggered after the page load
  // (e.g. onclick).
  // TODO(crbug.com/525224): Eliminate the use of jstemplate
  // in WebUI
  unsanitizedPolicy = trustedTypes.createPolicy(
      'jstemplate', {createScript: opaqueScript => opaqueScript});
}


/**
 * Cache for jsEvalToFunction results.
 * @type Object
 */
JsEvalContext.evalToFunctionCache_ = {};


/**
 * Evaluates the given expression as the body of a function that takes
 * vars and data as arguments. Since the resulting function depends
 * only on expr, we cache the result so we save some Function
 * invocations, and some object creations in IE6.
 *
 * @param {string} expr A javascript expression.
 *
 * @return {Function} A function that returns the value of expr in the
 * context of vars and data.
 */
function jsEvalToFunction(expr) {
  if (!JsEvalContext.evalToFunctionCache_[expr]) {
    try {
      /** @type {string} */
      const f = `(function(a_, b_) { with (a_) with (b_) return ${expr} })`;
      /** @type {!TrustedScript|string} */
      const opaqueExpr = window.trustedTypes ? unsanitizedPolicy.createScript(f) : f;

      // TODO(crbug.com/1087743): Support Function constructor in Trusted Types
      // TODO(crbug.com/1091600): Support TrustedScript type as an argument to
      // eval in Closure Compiler
      /** @suppress {checkTypes} */
      JsEvalContext.evalToFunctionCache_[expr] = window.eval(opaqueExpr);
    } catch (e) {
      log('jsEvalToFunction (' + expr + ') EXCEPTION ' + e);
    }
  }
  return JsEvalContext.evalToFunctionCache_[expr];
}


/**
 * Evaluates the given expression to itself. This is meant to pass
 * through string attribute values.
 *
 * @param {string} expr
 *
 * @return {string}
 */
function jsEvalToSelf(expr) {
  return expr;
}


/**
 * Parses the value of the jsvalues attribute in jstemplates: splits
 * it up into a map of labels and expressions, and creates functions
 * from the expressions that are suitable for execution by
 * JsEvalContext.jsexec(). All that is returned as a flattened array
 * of pairs of a String and a Function.
 *
 * @param {string} expr
 *
 * @return {Array}
 */
function jsEvalToValues(expr) {
  // TODO(mesch): It is insufficient to split the values by simply
  // finding semi-colons, as the semi-colon may be part of a string
  // constant or escaped.
  var ret = [];
  var values = expr.split(REGEXP_semicolon);
  for (var i = 0, I = jsLength(values); i < I; ++i) {
    var colon = values[i].indexOf(CHAR_colon);
    if (colon < 0) {
      continue;
    }
    var label = stringTrim(values[i].substr(0, colon));
    var value = jsEvalToFunction(values[i].substr(colon + 1));
    ret.push(label, value);
  }
  return ret;
}


/**
 * Parses the value of the jseval attribute of jstemplates: splits it
 * up into a list of expressions, and creates functions from the
 * expressions that are suitable for execution by
 * JsEvalContext.jsexec(). All that is returned as an Array of
 * Function.
 *
 * @param {string} expr
 *
 * @return {Array.<Function>}
 */
function jsEvalToExpressions(expr) {
  var ret = [];
  var values = expr.split(REGEXP_semicolon);
  for (var i = 0, I = jsLength(values); i < I; ++i) {
    if (values[i]) {
      var value = jsEvalToFunction(values[i]);
      ret.push(value);
    }
  }
  return ret;
}

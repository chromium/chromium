// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a system for memoizing computations applied to
 * DOM nodes within the same call stack.
 *
 * To make a function memoizable - suppose you have a function
 * isAccessible that takes a node and returns a boolean:
 *
 * function isAccessible(node) {
 *   return expensiveComputation(node);
 * }
 *
 * Make it memoizable like this:
 *
 * function isAccessible(node) {
 *   return cvox.Memoize.memoize(computeIsAccessible_, 'isAccessible', node);
 * }
 *
 * function computeIsAccessible_(node) {
 *   return expensiveComputation(node);
 * }
 *
 * To take advantage of memoization, you need to wrap a sequence of
 * computations in a call to memoize.scope() - memoization is only
 * enabled while in that scope, and all cached data is thrown away at
 * the end. You should use this only when you're sure the computation
 * being memoized will not change within the scope.
 *
 * cvox.Memoize.scope(function() {
 *   console.log(isAccessible(document.body));
 * });
 *
 */


goog.provide('cvox.Memoize');


/**
 * Create the namespace.
 * @constructor
 */
cvox.Memoize = function() {
};

/**
 * The cache: a map from string function name to a WeakMap from DOM node
 * to function result. This variable is null when we're out of scope, and it's
 * a map from string to WeakMap to result when we're in scope.
 *
 * @type {?Object<WeakMap<Node, *> >}
 * @private
 */
cvox.Memoize.nodeMap_ = null;

/**
 * Keeps track of how many nested times scope() has been called.
 * @type {number}
 * @private
 */
cvox.Memoize.scopeCount_ = 0;


/**
 * Enables memoization within the scope of the given function. You should
 * ensure that the DOM is not modified within this scope.
 *
 * It's safe to nest calls to scope. The nested calls have
 * no effect, only the outermost one.
 *
 * @param {Function} functionScope The function to call with memoization
 *     enabled.
 * @return {*} The value returned by |functionScope|.
 */
cvox.Memoize.scope = function(functionScope) {
  var result;
  try {
    cvox.Memoize.scopeCount_++;
    if (cvox.Memoize.scopeCount_ == 1) {
      cvox.Memoize.nodeMap_ = {};
    }
    result = functionScope();
  } finally {
    cvox.Memoize.scopeCount_--;
    if (cvox.Memoize.scopeCount_ == 0) {
      cvox.Memoize.nodeMap_ = null;
    }
  }
  return result;
};

/**
 * Memoizes the result of a function call, so if you call this again
 * with the same exact parameters and memoization is currently enabled
 * (via a call to scope()), the second time the cached result
 * will just be returned directly.
 *
 * @param {Function} functionClosure The function to call and cache the
 *     result of.
 * @param {string} functionName The name of the function you're calling.
 *     This string is used to store and retrieve the cached result, so
 *     it should be unique. If the function to be memoized takes simple
 *     arguments in addition to a DOM node, you can incorporate those
 *     arguments into the function name.
 * @param {Node} node The DOM node that should be passed as the argument
 *     to the function.
 * @return {*} The return value of |functionClosure|.
 */
cvox.Memoize.memoize = function(functionClosure, functionName, node) {
  if (cvox.Memoize.nodeMap_ &&
      cvox.Memoize.nodeMap_[functionName] === undefined) {
    cvox.Memoize.nodeMap_[functionName] = new WeakMap();
  }

  // If we're not in scope, just call the function directly.
  if (!cvox.Memoize.nodeMap_) {
    return functionClosure(node);
  }

  var result = cvox.Memoize.nodeMap_[functionName].get(node);
  if (result === undefined) {
    result = functionClosure(node);
    if (result === undefined) {
      throw 'A memoized function cannot return undefined.';
    }
    cvox.Memoize.nodeMap_[functionName].set(node, result);
  }

  return result;
};

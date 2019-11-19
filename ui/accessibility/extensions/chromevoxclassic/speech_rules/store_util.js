// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview General utility functions for rule stores.
 */

goog.provide('cvox.StoreUtil');


/**
 * Count list of nodes and concatenate this with the context string.
 * Returns a closure with a local state.
 * @param {Array<Node>} nodes A node array.
 * @param {?string} context A context string.
 * @return {function(): string} A function returning a string.
 */
cvox.StoreUtil.nodeCounter = function(nodes, context) {
  // Local state.
  var localLength = nodes.length;
  var localCounter = 0;
  var localContext = context;
  if (!context) {
    localContext = '';
  }
  return function() {
    if (localCounter < localLength) {
      localCounter += 1;
    }
    return localContext + ' ' + localCounter;
  };
};

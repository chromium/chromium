// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The cvox.NodeState typedef.
 */

goog.provide('cvox.NodeState');
goog.provide('cvox.NodeStateUtil');

goog.require('Msgs');

/**
 * Holds the state of a node.  It is an Array or Arrays of strings and numbers.
 * Each sub array is in the format:
 * [state, opt_arg, opt_arg, ...].  These sub arrays map directly to a
 * cvox.ChromeVox.getMsg() call. For example [list_position, 3, 5] maps to
 * getMsg('list_position', [3, 5]);
 *
 * @typedef {!Array<!Array<string|number>>}
 */
cvox.NodeState;

/**
 * Returns a localized, readable string with the NodeState.
 *
 * NOTE(deboer): Once AriaUtil and DomUtil are using NodeState exclusively, this
 * function can be moved into DescriptionUtil, removing the cvox.ChromeVox
 * dependency here.
 *
 * @param {cvox.NodeState} state The node state.
 * @return {string} The readable state string.
 */
cvox.NodeStateUtil.expand = function(state) {
  try {
    return state.map(function(s) {
      if (s.length < 1) {
        throw new Error('cvox.NodeState must have at least one entry');
      }
      var args = s.slice(1).map(function(a) {
        if (typeof a == 'number') {
          return Msgs.getNumber(a);
        }
        return a;
      });
      return Msgs.getMsg(/** @type {string} */ (s[0]), args);
    }).join(' ');
  } catch (e) {
    throw new Error('error: ' + e + ' state: ' + state);
  }
};

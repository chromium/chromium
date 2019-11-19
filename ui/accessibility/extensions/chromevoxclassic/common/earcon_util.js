// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcon utils.
 */

goog.provide('cvox.EarconUtil');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.AriaUtil');
goog.require('cvox.DomUtil');

/**
 * Returns the id of an earcon to play along with the description for a node.
 *
 * @param {Node} node The node to get the earcon for.
 * @return {cvox.Earcon?} The earcon id, or null if none applies.
 */
cvox.EarconUtil.getEarcon = function(node) {
  var earcon = cvox.AriaUtil.getEarcon(node);
  if (earcon != null) {
    return earcon;
  }

  switch (node.tagName) {
    case 'BUTTON':
      return cvox.Earcon.BUTTON;
    case 'A':
      if (node.hasAttribute('href')) {
        return cvox.Earcon.LINK;
      }
      break;
    case 'IMG':
      if (cvox.DomUtil.hasLongDesc(node)) {
        return cvox.Earcon.LONG_DESC;
      }
      break;
    case 'LI':
      return cvox.Earcon.LIST_ITEM;
    case 'SELECT':
      return cvox.Earcon.LISTBOX;
    case 'TEXTAREA':
      return cvox.Earcon.EDITABLE_TEXT;
    case 'INPUT':
      switch (node.type) {
        case 'button':
        case 'submit':
        case 'reset':
          return cvox.Earcon.BUTTON;
        case 'checkbox':
        case 'radio':
          if (node.checked) {
            return cvox.Earcon.CHECK_ON;
          } else {
            return cvox.Earcon.CHECK_OFF;
          }
        default:
          if (cvox.DomUtil.isInputTypeText(node)) {
            // 'text', 'password', etc.
            return cvox.Earcon.EDITABLE_TEXT;
          }
      }
  }
  return null;
};

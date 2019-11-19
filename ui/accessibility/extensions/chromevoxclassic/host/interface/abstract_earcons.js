// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for implementing earcons.
 *
 * When adding earcons, please add them to getEarconName and getEarconId.
 *
 */

goog.provide('cvox.AbstractEarcons');
goog.provide('cvox.Earcon');


/**
 * Earcon names.
 * @enum {string}
 */
cvox.Earcon = {
  ALERT_MODAL: 'alert_modal',
  ALERT_NONMODAL: 'alert_nonmodal',
  BUTTON: 'button',
  CHECK_OFF: 'check_off',
  CHECK_ON: 'check_on',
  EDITABLE_TEXT: 'editable_text',
  INVALID_KEYPRESS: 'invalid_keypress',
  LINK: 'link',
  LISTBOX: 'listbox',
  LIST_ITEM: 'list_item',
  LONG_DESC: 'long_desc',
  MATH: 'math',
  OBJECT_CLOSE: 'object_close',
  OBJECT_ENTER: 'object_enter',
  OBJECT_EXIT: 'object_exit',
  OBJECT_OPEN: 'object_open',
  OBJECT_SELECT: 'object_select',
  PAGE_FINISH_LOADING: 'page_finish_loading',
  PAGE_START_LOADING: 'page_start_loading',
  POP_UP_BUTTON: 'pop_up_button',
  RECOVER_FOCUS: 'recover_focus',
  SELECTION: 'selection',
  SELECTION_REVERSE: 'selection_reverse',
  SKIP: 'skip',
  SLIDER: 'slider',
  WRAP: 'wrap',
  WRAP_EDGE: 'wrap_edge',
};


/**
 * @constructor
 */
cvox.AbstractEarcons = function() {
};


/**
 * Public static flag set to enable or disable earcons. Callers should prefer
 * toggle(); however, this member is public for initialization.
 * @type {boolean}
 */
cvox.AbstractEarcons.enabled = true;


/**
 * Plays the specified earcon sound.
 * @param {cvox.Earcon} earcon An earcon identifier.
 */
cvox.AbstractEarcons.prototype.playEarcon = function(earcon) {
};


/**
 * Cancels the specified earcon sound.
 * @param {cvox.Earcon} earcon An earcon identifier.
 */
cvox.AbstractEarcons.prototype.cancelEarcon = function(earcon) {
};


/**
 * Whether or not earcons are available.
 * @return {boolean} True if earcons are available.
 */
cvox.AbstractEarcons.prototype.earconsAvailable = function() {
  return true;
};


/**
 * Toggles earcons on or off.
 * @return {boolean} True if earcons are now enabled; false otherwise.
 */
cvox.AbstractEarcons.prototype.toggle = function() {
  cvox.AbstractEarcons.enabled = !cvox.AbstractEarcons.enabled;
  return cvox.AbstractEarcons.enabled;
};

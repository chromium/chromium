// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses EarconEngine to play back
 * auditory cues.
 */


goog.provide('NextEarcons');

goog.require('EarconEngine');
goog.require('cvox.AbstractEarcons');


/**
 * @constructor
 * @extends {cvox.AbstractEarcons}
 */
NextEarcons = function() {
  cvox.AbstractEarcons.call(this);

  if (localStorage['earcons'] === 'false') {
    cvox.AbstractEarcons.enabled = false;
  }

  /**
   * @type {EarconEngine}
   * @private
   */
  this.engine_ = new EarconEngine();
};

NextEarcons.prototype = {
  /**
   * @return {string} The human-readable name of the earcon set.
   */
  getName: function() {
    return 'ChromeVox Next earcons';
  },

  /**
   * @override
   */
  playEarcon: function(earcon) {
    if (!cvox.AbstractEarcons.enabled) {
      return;
    }
    console.log('Earcon ' + earcon);

    switch (earcon) {
      case cvox.Earcon.ALERT_MODAL:
      case cvox.Earcon.ALERT_NONMODAL:
        this.engine_.onAlert();
        break;
      case cvox.Earcon.BUTTON:
        this.engine_.onButton();
        break;
      case cvox.Earcon.CHECK_OFF:
        this.engine_.onCheckOff();
        break;
      case cvox.Earcon.CHECK_ON:
        this.engine_.onCheckOn();
        break;
      case cvox.Earcon.EDITABLE_TEXT:
        this.engine_.onTextField();
        break;
      case cvox.Earcon.INVALID_KEYPRESS:
        this.engine_.onWrap();
        break;
      case cvox.Earcon.LINK:
        this.engine_.onLink();
        break;
      case cvox.Earcon.LISTBOX:
        this.engine_.onSelect();
        break;
      case cvox.Earcon.LIST_ITEM:
      case cvox.Earcon.LONG_DESC:
      case cvox.Earcon.MATH:
      case cvox.Earcon.OBJECT_CLOSE:
      case cvox.Earcon.OBJECT_ENTER:
      case cvox.Earcon.OBJECT_EXIT:
      case cvox.Earcon.OBJECT_OPEN:
      case cvox.Earcon.OBJECT_SELECT:
        // TODO(dmazzoni): decide if we want new earcons for these
        // or not. We may choose to not have earcons for some of these.
        break;
      case cvox.Earcon.PAGE_FINISH_LOADING:
        this.engine_.cancelProgress();
        break;
      case cvox.Earcon.PAGE_START_LOADING:
        this.engine_.startProgress();
        break;
      case cvox.Earcon.POP_UP_BUTTON:
        this.engine_.onPopUpButton();
        break;
      case cvox.Earcon.RECOVER_FOCUS:
        // TODO(dmazzoni): decide if we want new earcons for this.
        break;
      case cvox.Earcon.SELECTION:
        this.engine_.onSelection();
        break;
      case cvox.Earcon.SELECTION_REVERSE:
        this.engine_.onSelectionReverse();
        break;
      case cvox.Earcon.SKIP:
        this.engine_.onSkim();
        break;
      case cvox.Earcon.SLIDER:
        this.engine_.onSlider();
        break;
      case cvox.Earcon.WRAP:
      case cvox.Earcon.WRAP_EDGE:
        this.engine_.onWrap();
        break;
    }
  },

  /**
   * @override
   */
  cancelEarcon: function(earcon) {
    switch (earcon) {
      case cvox.Earcon.PAGE_START_LOADING:
        this.engine_.cancelProgress();
        break;
    }
  },
};

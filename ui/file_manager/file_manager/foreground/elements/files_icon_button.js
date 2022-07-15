// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_ripple.js';
import './files_toggle_ripple.js';

import {IronButtonState} from 'chrome://resources/polymer/v3_0/iron-behaviors/iron-button-state.js';
import {IronControlState} from 'chrome://resources/polymer/v3_0/iron-behaviors/iron-control-state.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,

  is: 'files-icon-button',

  hostAttributes: {
    role: 'button',
    tabindex: 0,
  },

  behaviors: [
    IronButtonState,
    IronControlState,
  ],

  observers: [
    '_focusedChanged(receivedFocusFromKeyboard)',
  ],

  _focusedChanged: function(receivedFocusFromKeyboard) {
    if (receivedFocusFromKeyboard) {
      this.classList.add('keyboard-focus');
    } else {
      this.classList.remove('keyboard-focus');
    }
  },
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_icon_button.js

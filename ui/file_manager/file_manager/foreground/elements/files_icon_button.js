// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'files-icon-button',

  hostAttributes: {
    role: 'button',
    tabindex: 0
  },

  behaviors: [
    Polymer.IronButtonState,
    Polymer.IronControlState
  ],

  observers: [
    '_focusedChanged(receivedFocusFromKeyboard)'
  ],

  _focusedChanged: function(receivedFocusFromKeyboard) {
    if (receivedFocusFromKeyboard) {
      this.classList.add('keyboard-focus');
    } else {
      this.classList.remove('keyboard-focus');
    }
  }
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_icon_button.js

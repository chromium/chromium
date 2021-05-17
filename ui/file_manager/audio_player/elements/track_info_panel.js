// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


Polymer({
  _template: html`{__html_template__}`,

  is: 'track-info-panel',

  properties: {
    track: {
      type: Object,
      value: null,
    },

    expanded: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
      observer: 'onExpandedChanged_',
    },

    artworkAvailable: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    ariaExpandArtworkLabel: String,
  },

  /** @private */
  onExpandClick_: function() {
    this.expanded = !this.expanded;
  },

  /** @private */
  onExpandedChanged_: function() {
    this.$.expand.setAttribute('aria-expanded', Boolean(this.expanded));
  },
});

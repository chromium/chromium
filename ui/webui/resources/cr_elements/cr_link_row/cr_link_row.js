// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 */
import '../cr_actionable_row_style.m.js';
import '../cr_icon_button/cr_icon_button.m.js';
import '../hidden_style_css.m.js';
import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class CrLinkRowElement extends PolymerElement {
  static get is() {
    return 'cr-link-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      startIcon: {
        type: String,
        value: '',
      },

      label: {
        type: String,
        value: '',
      },

      subLabel: {
        type: String,
        /* Value used for noSubLabel attribute. */
        value: '',
      },

      disabled: {
        type: Boolean,
        reflectToAttribute: true,
      },

      external: {
        type: Boolean,
        value: false,
      },

      usingSlottedLabel: {
        type: Boolean,
        value: false,
      },

      roleDescription: String,

      /** @private */
      hideLabelWrapper_: {
        type: Boolean,
        computed: 'computeHideLabelWrapper_(label, usingSlottedLabel)',
      },
    };
  }

  /** @type {boolean} */
  get noink() {
    return this.$.icon.noink;
  }

  /** @type {boolean} */
  set noink(value) {
    this.$.icon.noink = value;
  }

  focus() {
    this.$.icon.focus();
  }

  /**
   * @return {boolean}
   * @private
   */
  computeHideLabelWrapper_() {
    return !(this.label || this.usingSlottedLabel);
  }

  /**
   * @return {string}
   * @private
   */
  getIcon_() {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  }
}

customElements.define(CrLinkRowElement.is, CrLinkRowElement);

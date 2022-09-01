// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.m.js';
import '../cr_shared_style.css.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class CrTooltipIconElement extends PolymerElement {
  static get is() {
    return 'cr-tooltip-icon';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      iconAriaLabel: String,
      iconClass: String,
      tooltipText: String,

      /** Position of tooltip popup related to the icon. */
      tooltipPosition: {
        type: String,
        value: 'top',
      },
    };
  }

  /** @return {!Element} */
  getFocusableElement() {
    return this.$.indicator;
  }
}

customElements.define(CrTooltipIconElement.is, CrTooltipIconElement);

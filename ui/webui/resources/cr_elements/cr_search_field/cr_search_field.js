// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldBehavior.
 */

import '../cr_icon_button/cr_icon_button.m.js';
import '../cr_input/cr_input.m.js';
import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSearchFieldBehavior, CrSearchFieldBehaviorInterface} from './cr_search_field_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrSearchFieldBehaviorInterface}
 */
const CrSearchFieldElementBase =
    mixinBehaviors([CrSearchFieldBehavior], PolymerElement);

/** @polymer */
export class CrSearchFieldElement extends CrSearchFieldElementBase {
  static get is() {
    return 'cr-search-field';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      autofocus: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @return {!HTMLInputElement} */
  getSearchInput() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  }

  /** @private */
  onTapClear_() {
    this.setValue('');
    setTimeout(() => {
      this.$.searchInput.focus();
    });
  }
}

customElements.define(CrSearchFieldElement.is, CrSearchFieldElement);

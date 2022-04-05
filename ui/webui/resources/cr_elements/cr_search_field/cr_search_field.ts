// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldMixin.
 */

import '../cr_icon_button/cr_icon_button.m.js';
import '../cr_input/cr_input.m.js';
import '../cr_input/cr_input_style_css.m.js';
import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrInputElement} from '../cr_input/cr_input.m.js';

import {CrSearchFieldMixin} from './cr_search_field_mixin.js';

const CrSearchFieldElementBase = CrSearchFieldMixin(PolymerElement);

export interface CrSearchFieldElement {
  $: {
    clearSearch: HTMLElement,
    searchInput: CrInputElement,
  };
}

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

  override autofocus: boolean;

  override getSearchInput(): CrInputElement {
    return this.$.searchInput;
  }

  private onTapClear_() {
    this.setValue('');
    setTimeout(() => {
      this.$.searchInput.focus();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-search-field': CrSearchFieldElement;
  }
}

customElements.define(CrSearchFieldElement.is, CrSearchFieldElement);

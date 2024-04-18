// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldMixin.
 */

import '../cr_icon_button/cr_icon_button.js';
import '../cr_input/cr_input.js';
import '../cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrInputElement} from '../cr_input/cr_input.js';

import {getCss} from './cr_search_field.css.js';
import {getHtml} from './cr_search_field.html.js';
import {CrSearchFieldMixinLit} from './cr_search_field_mixin_lit.js';

const CrSearchFieldElementBase = CrSearchFieldMixinLit(CrLitElement);

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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      autofocus: {
        type: Boolean,
      },
    };
  }

  override autofocus: boolean = false;

  override getSearchInput(): CrInputElement {
    return this.$.searchInput;
  }

  protected onClearSearchClick_() {
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

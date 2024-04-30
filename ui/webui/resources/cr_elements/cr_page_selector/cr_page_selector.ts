// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_page_selector.css.js';
import {getHtml} from './cr_page_selector.html.js';
import {CrSelectableMixin} from '../cr_selectable_mixin.js';

const CrPageSelectorElementBase = CrSelectableMixin(CrLitElement);

export class CrPageSelectorElement extends CrPageSelectorElementBase {
  static get is() {
    return 'cr-page-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-page-selector': CrPageSelectorElement;
  }
}

customElements.define(CrPageSelectorElement.is, CrPageSelectorElement);

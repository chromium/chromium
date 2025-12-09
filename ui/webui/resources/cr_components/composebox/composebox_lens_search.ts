// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox_tab_favicon.js';
import './icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_lens_search.css.js';
import {getHtml} from './composebox_lens_search.html.js';

export class ComposeboxLensSearchElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'cr-composebox-lens-search';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onLensSearchClick_(e: Event) {
    e.stopPropagation();
    this.fire('lens-search-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-lens-search': ComposeboxLensSearchElement;
  }
}
customElements.define(
    ComposeboxLensSearchElement.is, ComposeboxLensSearchElement);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './searchbox_compose_button.css.js';
import {getHtml} from './searchbox_compose_button.html.js';

export class SearchboxComposeButtonElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-compose-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      composeIcon_: {
        type: String,
        reflect: true,
      },
    };
  }

  protected accessor composeIcon_: string =
      '//resources/cr_components/searchbox/icons/search_spark.svg';

  protected onClick_(e: MouseEvent) {
    e.preventDefault();
    this.fire('compose-click', {
      button: e.button,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-compose-button': SearchboxComposeButtonElement;
  }
}

customElements.define(
    SearchboxComposeButtonElement.is, SearchboxComposeButtonElement);

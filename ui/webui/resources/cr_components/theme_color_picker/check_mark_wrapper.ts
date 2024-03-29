// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './check_mark_wrapper.css.js';
import {getHtml} from './check_mark_wrapper.html.js';

export interface CheckMarkWrapperElement {
  $: {
    svg: SVGElement,
  };
}

export class CheckMarkWrapperElement extends CrLitElement {
  static get is() {
    return 'cr-theme-color-check-mark-wrapper';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      checked: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  checked: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-color-check-mark-wrapper': CheckMarkWrapperElement;
  }
}

customElements.define(CheckMarkWrapperElement.is, CheckMarkWrapperElement);

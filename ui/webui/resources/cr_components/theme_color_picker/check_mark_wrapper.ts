// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './check_mark_wrapper.html.js';

export interface CheckMarkWrapperElement {
  $: {
    svg: Element,
  };
}

export class CheckMarkWrapperElement extends PolymerElement {
  static get is() {
    return 'cr-theme-color-check-mark-wrapper';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-color-check-mark-wrapper': CheckMarkWrapperElement;
  }
}

customElements.define(CheckMarkWrapperElement.is, CheckMarkWrapperElement);

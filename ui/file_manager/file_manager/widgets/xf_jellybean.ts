// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which controls pre- and post-jellybean migration UI.
 */

import {isCrosComponentsEnabled} from '../common/js/flags.js';

import {customElement, html, XfBase} from './xf_base.js';

@customElement('xf-jellybean')
export class XfJellybean extends XfBase {
  override render() {
    if (isCrosComponentsEnabled()) {
      return html`
        <slot name="jelly">
          Jelly
        <slot>
      `;
    }

    return html`
      <slot name="old">
        Big Belly
      <slot>`;
  }

  override firstUpdated() {
    // Jellybean status does not change during runtime. We can cleanup the
    // unused variant.
    const unusedElements = isCrosComponentsEnabled() ?
        this.querySelectorAll('[slot="old"]') :
        this.querySelectorAll('[slot="jelly"]');
    unusedElements.forEach((el: Element) => el.remove());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-jellybean': XfJellybean;
  }
}

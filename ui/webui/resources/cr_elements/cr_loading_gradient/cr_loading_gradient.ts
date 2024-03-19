// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_loading_gradient.css.js';
import {getHtml} from './cr_loading_gradient.html.js';

/* Count of cr-loading-gradient elements created. Used to assign unique IDs.
 * Unique IDs are necessary since clipPaths are slotted in from the light DOM,
 * so there can be leakages across multiple <cr-loading-gradient> instances. */
let count = 0;

export class CrLoadingGradientElement extends CrLitElement {
  static get is() {
    return 'cr-loading-gradient';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onSlotchange_() {
    const clipPath = this.querySelector('svg clipPath');
    assert(clipPath);
    const generatedId = `crLoadingGradient${count++}`;
    clipPath.id = generatedId;
    this.style.clipPath = `url(#${generatedId})`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-loading-gradient': CrLoadingGradientElement;
  }
}

customElements.define(CrLoadingGradientElement.is, CrLoadingGradientElement);

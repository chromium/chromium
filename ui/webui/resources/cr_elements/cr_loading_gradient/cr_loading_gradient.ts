// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_loading_gradient.html.js';

/* Count of cr-loading-gradient elements created. Used to assign unique IDs.
 * Unique IDs are necessary since clipPaths are slotted in from the light DOM,
 * so there can be leakages across multiple <cr-loading-gradient> instances. */
let count = 0;

export class CrLoadingGradientElement extends PolymerElement {
  static get is() {
    return 'cr-loading-gradient';
  }

  static get template() {
    return getTemplate();
  }

  private onSlotchange_() {
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

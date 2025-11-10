/**
 * Copyright 2025 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './animated_glow.css.js';
import {getHtml} from './animated_glow.html.js';
import {GlowAnimationState} from './constants.js';

export class SearchAnimatedGlowElement extends CrLitElement {
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      animationState: {
        type: String,
        reflect: true,
      },
    };
  }

  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
}

declare global {
  interface HTMLElementTagNameMap {
    'search-animated-glow': SearchAnimatedGlowElement;
  }
}

customElements.define('search-animated-glow', SearchAnimatedGlowElement);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './animated_glow.css.js';
import {getHtml} from './animated_glow.html.js';
import {GlowAnimationState} from './constants.js';

export class SearchAnimatedGlowElement extends CrLitElement {
  static get is() {
    return 'search-animated-glow';
  }

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
      entrypointName: {type: String},
    };
  }

  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  dragDropPlaceholder: string =
      loadTimeData.getString('composeboxDragAndDropHint');
}

declare global {
  interface HTMLElementTagNameMap {
    'search-animated-glow': SearchAnimatedGlowElement;
  }
}

customElements.define(SearchAnimatedGlowElement.is, SearchAnimatedGlowElement);

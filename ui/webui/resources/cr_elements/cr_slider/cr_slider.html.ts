// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrSliderElement} from './cr_slider.js';

export function getHtml(this: CrSliderElement) {
  return html`
<div id="container" part="container">
  <div id="bar"></div>
  <div id="markers" ?hidden="${!this.markerCount}">
    ${this.getMarkers_().map((_item, index) => html`
      <div class="${this.getMarkerClass_(index)}"></div>
    `)}
  </div>
  <div id="knobAndLabel" @transitionend="${this.onTransitionEnd_}">
    <div id="knob" part="knob"></div>
    <div id="label" part="label">${this.label_}</div>
  </div>
</div>`;
}

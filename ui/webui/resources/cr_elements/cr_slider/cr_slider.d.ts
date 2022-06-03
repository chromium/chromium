// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export type SliderTick = {
  value: number,
  label: string,
  ariaValue?: number,
}

interface CrSliderElement extends LegacyElementMixin, HTMLElement {
  disabled: boolean;
  dragging: boolean;
  updatingFromKey: boolean;
  markerCount: number;
  max: number;
  min: number;
  noKeybindings: boolean;
  snaps: boolean;
  ticks: Array<SliderTick>|Array<number>;
  value: number;
}

export {CrSliderElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-slider': CrSliderElement;
  }
}

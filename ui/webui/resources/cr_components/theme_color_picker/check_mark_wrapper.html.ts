// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CheckMarkWrapperElement} from './check_mark_wrapper.js';

export function getHtml(this: CheckMarkWrapperElement) {
  return html`
<svg id="svg" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
  <circle id="background" cx="24" cy="24" r="24"></circle>
  <path id="checkMark" d="M20 34 10 24l2.83-2.83L20 28.34l15.17-15.17L38 16Z">
  </path>
</svg>
<slot></slot>`;
}

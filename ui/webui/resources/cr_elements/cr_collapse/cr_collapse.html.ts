// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrCollapseElement} from './cr_collapse.js';

export function getHtml(this: CrCollapseElement) {
  return html`<slot></slot>`;
}

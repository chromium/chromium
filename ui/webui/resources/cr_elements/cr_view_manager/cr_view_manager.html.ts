// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrViewManagerElement} from './cr_view_manager.js';

export function getHtml(this: CrViewManagerElement) {
  return html`<slot name="view"></slot>`;
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrProgressElement} from './cr_progress.js';

export function getHtml(this: CrProgressElement) {
  return html`
    <div id="progressContainer">
      <div id="primaryProgress"></div>
    </div>`;
}

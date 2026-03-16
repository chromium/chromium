// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-element-structure

export class InconsistentFilenameElement extends CrLitElement {
  static get is() {
    return 'inconsistent-filename';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'inconsistent-filename': InconsistentFilenameElement;
  }
}

customElements.define(
    InconsistentFilenameElement.is, InconsistentFilenameElement);

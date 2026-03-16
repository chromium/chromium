// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-element-structure

export class IncorrectFilenameSuffixElement extends CrLitElement {
  static get is() {
    return 'incorrect-filename-suffix';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'incorrect-filename-suffix': IncorrectFilenameSuffixElement;
  }
}

customElements.define(
    IncorrectFilenameSuffixElement.is, IncorrectFilenameSuffixElement);

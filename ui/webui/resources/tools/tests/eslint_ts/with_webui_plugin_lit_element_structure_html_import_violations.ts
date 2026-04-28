// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

export class TestHtmlImportErrorElement extends CrLitElement {
  static get is() {
    return 'test-html-import-error';
  }

  override render() {
    return html`<div></div>`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-html-import-error': TestHtmlImportErrorElement;
  }
}

customElements.define(
    TestHtmlImportErrorElement.is, TestHtmlImportErrorElement);

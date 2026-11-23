// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-member-visibility (inline render())

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

export class InlineTemplateDummyTestElement extends CrLitElement {
  static get is() {
    return 'inline-template-dummy-test';
  }

  override render() {
    return html`<div>${this.usedProtectedInlineAccessor_}</div>`;
  }

  static override get properties() {
    return {
      usedProtectedInlineAccessor_: {type: String},
      unusedProtectedInlineAccessor_: {type: String},
    };
  }

  protected accessor usedProtectedInlineAccessor_: string = '';
  protected accessor unusedProtectedInlineAccessor_: string = '';
}

customElements.define(
    InlineTemplateDummyTestElement.is, InlineTemplateDummyTestElement);

export class NoTemplateDummyTestElement extends CrLitElement {
  static get is() {
    return 'no-template-dummy-test';
  }

  protected unusedProtectedNoTemplateProp_: string = '';
}

customElements.define(
    NoTemplateDummyTestElement.is, NoTemplateDummyTestElement);

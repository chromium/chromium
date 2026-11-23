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
    return html`
      <div>${this.usedProtectedInlineAccessor_}</div>
      <div>${this.usedProtectedInlineMethod_()}</div>
      <div>${this.usedProtectedInlineGetter_}</div>
    `;
  }

  static override get properties() {
    return {
      usedProtectedInlineAccessor_: {type: String},
      unusedProtectedInlineAccessor_: {type: String},
    };
  }

  protected accessor usedProtectedInlineAccessor_: string = '';
  protected accessor unusedProtectedInlineAccessor_: string = '';

  protected usedProtectedInlineMethod_(): string {
    return 'used';
  }

  protected get usedProtectedInlineGetter_(): string {
    return 'used';
  }

  protected unusedProtectedInlineMethod_(): string {
    return 'unused';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'inline-template-dummy-test': InlineTemplateDummyTestElement;
  }
}

customElements.define(
    InlineTemplateDummyTestElement.is, InlineTemplateDummyTestElement);

export class NoTemplateDummyTestElement extends CrLitElement {
  static get is() {
    return 'no-template-dummy-test';
  }

  protected unusedProtectedNoTemplateProp_: string = '';

  protected unusedProtectedNoTemplateMethod_(): void {}
}

declare global {
  interface HTMLElementTagNameMap {
    'no-template-dummy-test': NoTemplateDummyTestElement;
  }
}

customElements.define(
    NoTemplateDummyTestElement.is, NoTemplateDummyTestElement);

// Concrete CrLitElement class extended by another class: its protected members
// should not be flagged, whereas unused protected members on the leaf subclass
// should still be flagged.
export class ExtendedConcreteParentElement extends CrLitElement {
  static get is() {
    return 'extended-concrete-parent';
  }

  protected parentProtectedProp_: string = 'parent';

  protected parentProtectedMethod_(): string {
    return this.parentProtectedProp_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extended-concrete-parent': ExtendedConcreteParentElement;
  }
}

customElements.define(
    ExtendedConcreteParentElement.is, ExtendedConcreteParentElement);

export class ExtendedConcreteChildElement extends
    ExtendedConcreteParentElement {
  static override get is() {
    return 'extended-concrete-child';
  }

  callParentMethod(): string {
    return this.parentProtectedMethod_();
  }

  protected unusedChildProtectedMethod_(): void {}
}

declare global {
  interface HTMLElementTagNameMap {
    'extended-concrete-child': ExtendedConcreteChildElement;
  }
}

customElements.define(
    ExtendedConcreteChildElement.is, ExtendedConcreteChildElement);

export class ExtendedViaExprParentElement extends CrLitElement {
  static get is() {
    return 'extended-via-expr-parent';
  }

  protected exprParentMethod_(): string {
    return 'parent';
  }
}

export const ExtendedViaExprChildElement =
    class extends ExtendedViaExprParentElement {
  static override get is() {
    return 'extended-via-expr-child';
  }

  callParentMethod(): string {
    return this.exprParentMethod_();
  }
};

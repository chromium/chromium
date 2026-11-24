// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-member-visibility

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_lit_member_visibility_violations.html.js';

type Constructor<T> = new (...args: any[]) => T;
export const DummyMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T => {
      class DummyMixin extends superClass {
        protected mixinProp_: string = 'mixin';
        protected mixinMethod_(): string {
          return this.mixinProp_;
        }
      }
      return DummyMixin;
    };

export abstract class BaseDummyElement extends DummyMixin
(CrLitElement) {
  protected baseProtectedProp_: string = 'base';
  protected unusedBaseProp_: string = 'unused in base';
  protected baseProtectedMethod_(): string {
    return this.baseProtectedProp_;
  }
  protected abstract baseAbstractMethod_(): void;
}

export class WithWebuiPluginLitMemberVisibilityViolationsElement extends
    BaseDummyElement {
  static get is() {
    return 'with-webui-plugin-lit-member-visibility-violations';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      publicAccessor: {type: String},
      usedProtectedAccessor_: {type: String},
      usedInSubTemplateAccessor_: {type: String},
      usedInNestedExpr_: {type: Object},
      reflectedProtectedAccessor_: {type: Boolean, reflect: true},
      unusedProtectedAccessor_: {type: String},
      privateAccessor_: {type: String},
    };
  }

  accessor publicAccessor: string = '';
  protected accessor usedProtectedAccessor_: string = '';
  protected accessor usedInSubTemplateAccessor_: string = '';
  protected accessor usedInNestedExpr_: {label: string} = {label: ''};
  protected accessor reflectedProtectedAccessor_: boolean = false;
  protected accessor unusedProtectedAccessor_: string = '';
  private accessor privateAccessor_: string = '';

  protected readonly usedProtectedProp_: string = '';
  protected unusedProtectedProp_: number = 0;
  protected override baseProtectedProp_: string = 'overridden';

  protected constructor() {
    super();
  }

  protected override baseProtectedMethod_(): string {
    return 'overridden';
  }

  // Implementing an abstract base class method does not require `override`
  // even under `noImplicitOverride`.
  protected baseAbstractMethod_(): void {}

  protected onButtonClick_(): void {}

  protected usedProtectedMethod_(): string {
    return this.privateAccessor_;
  }

  protected usedInSubTemplateMethod_(): string {
    return 'sub';
  }

  protected unusedProtectedMethod_(): string {
    return 'unused';
  }

  protected get unusedProtectedGetter_(): string {
    return 'unused';
  }

  protected set unusedProtectedSetter_(_val: string) {}

  private privateMethod_(): string {
    return this.privateAccessor_;
  }

  callPrivateForTest(): string {
    return this.privateMethod_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'with-webui-plugin-lit-member-visibility-violations':
        WithWebuiPluginLitMemberVisibilityViolationsElement;
  }
}

customElements.define(
    WithWebuiPluginLitMemberVisibilityViolationsElement.is,
    WithWebuiPluginLitMemberVisibilityViolationsElement);

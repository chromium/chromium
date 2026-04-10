// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '/resources/lit/v3_0/lit.rollup.js';

import {MyTestMixin} from './with_webui_plugin_lit_element_bindings_violations_mixin.js';
import type {MyTestMixinInterface} from './with_webui_plugin_lit_element_bindings_violations_mixin.js';
import {getHtml} from './with_webui_plugin_lit_reactive_properties_violations.html.js';

const SomeFooElementBase = MyTestMixin(CrLitElement);

export interface SomeFooElement extends MyTestMixinInterface {}

export class SomeFooElement extends SomeFooElementBase {
  static get is() {
    return 'some-foo';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      propInProperties: {type: String},
    };
  }

  accessor propInProperties: string = 'in properties';
  propNotInProperties: string = 'not in properties';
  get getterProp(): string {
    return 'getter';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'some-foo': SomeFooElement;
  }
}

customElements.define(SomeFooElement.is, SomeFooElement);

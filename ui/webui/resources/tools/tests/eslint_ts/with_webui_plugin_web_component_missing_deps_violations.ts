// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/web-component-missing-deps

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './other_button.js';
import './my_foo.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_web_component_missing_deps_violations.html.js';

export class MyDummyElement extends CrLitElement {
  static get is() {
    return 'test-dummy';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

customElements.define(MyDummyElement.is, MyDummyElement);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/web-component-missing-deps

import type {MyDummyElement} from './with_webui_plugin_web_component_missing_deps_violations.js';
import {getHtml as getDummyBarHtml} from './with_webui_plugin_web_component_missing_deps_violations_subcomponent_bar.html.js';
import {getHtml as getDummyFooHtml} from './with_webui_plugin_web_component_missing_deps_violations_subcomponent_foo.html.js';

export function getHtml(this: MyDummyElement) {
  return html`
${getDummyFooHtml.bind(this)()}
<cr-icon-button></cr-icon-button>
${getDummyBarHtml.bind(this)()}
<other-button1 id="button1"></other-button1>
<other-button2
    id="button2"></other-button2>
<cr-expand-button></cr-expand-button>
${getDummyFooHtml.bind(this)()}
<some-other-button id="button3"></some-other-button>
<iron-list></iron-list>
${getDummyBarHtml.bind(this)()}
<foo-bar></foo-bar>
`;
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WithWebuiPluginLitMemberVisibilityViolationsElement} from './with_webui_plugin_lit_member_visibility_violations.js';
import {getHtml as getSubHtml} from './with_webui_plugin_lit_member_visibility_violations_sub.html.js';

export function getHtml(
    this: WithWebuiPluginLitMemberVisibilityViolationsElement) {
  return html`
    <div>${this.usedProtectedAccessor_}</div>
    <div>${this.usedProtectedProp_}</div>
    <div>${this.usedInNestedExpr_.label}</div>
    ${getSubHtml.bind(this)()}
  `;
}

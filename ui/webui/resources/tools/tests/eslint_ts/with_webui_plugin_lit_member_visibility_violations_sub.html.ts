// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WithWebuiPluginLitMemberVisibilityViolationsElement} from './with_webui_plugin_lit_member_visibility_violations.js';

export function getHtml(
    this: WithWebuiPluginLitMemberVisibilityViolationsElement) {
  return html`<div>${this.usedInSubTemplateAccessor_}</div>`;
}

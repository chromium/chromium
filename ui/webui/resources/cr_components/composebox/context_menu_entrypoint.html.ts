// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextMenuEntrypointElement} from './context_menu_entrypoint.js';

export function getHtml(this: ContextMenuEntrypointElement) {
  return html`<!--_html_template_start_-->
<cr-button id="contextButton"
    title="$i18n{addContextTitle}">
  <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
  <span id="description">$i18n{addContext}</span>
</cr-button>
<!--_html_template_end_-->`;
}

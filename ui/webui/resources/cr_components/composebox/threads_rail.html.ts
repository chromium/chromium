// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {type ThreadsRailElement} from './threads_rail.js';

export function getHtml(this: ThreadsRailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="rail">
    ${this.displayLogo_ ? html`
      <img src="chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg"
          id="logo">
    ` : ''}
    <!-- Note: The icon is the same as the one used for AI Mode, but exposed to
         non-branded chrome builds as well. -->
    <cr-icon-button id="showHistoryButton"
        iron-icon="composebox:threadsHistory"
        @click="${this.onShowHistoryClick_}"
        title="${this.i18n('aimThreadsHistoryLabel')}">
    </cr-icon-button>
  </div>
  <!--_html_template_end_-->`;
  // clang-format on;
}

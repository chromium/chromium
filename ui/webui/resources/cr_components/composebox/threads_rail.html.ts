// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {type ThreadsRailElement} from './threads_rail.js';

export function getHtml(this: ThreadsRailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="rail">
    <cr-icon-button id="logoButton"
        suppress-rtl-flip
        @click="${this.onLogoClick_}"
        aria-label="${this.i18n('aimThreadsGoogleSearchLabel')}">
    </cr-icon-button>
    <cr-tooltip for="logoButton" position="right" offset="16"
        animation-delay="0" hide-delay="0">
      ${this.i18n('aimThreadsGoogleSearchLabel')}
    </cr-tooltip>
    <div id="iconsContainer">
      ${this.getItems_().map((item, index) => html`
        <cr-icon-button id="railItem-${index}"
            data-index="${index}"
            style="${this.getIconStyle_(item) || nothing}"
            @click="${this.onItemClick_}"
            aria-label="${item.name || nothing}">
        </cr-icon-button>
        ${item.name ? html`
          <cr-tooltip for="railItem-${index}" position="right"
              offset="16" animation-delay="0" hide-delay="0">
            ${item.name}
          </cr-tooltip>
        ` : nothing}
      `)}
    </div>
  </div>
  <!--_html_template_end_-->`;
  // clang-format on
}

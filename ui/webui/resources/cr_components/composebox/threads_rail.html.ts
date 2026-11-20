// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {type ThreadsRailElement} from './threads_rail.js';

export function getHtml(this: ThreadsRailElement) {
  return html`<!--_html_template_start_-->
<div id="rail" @click="${this.onRailClick_}">
  <cr-icon-button id="logoButton" suppress-rtl-flip
      @click="${this.onLogoClick_}"
      title="${this.i18n('aimThreadsGoogleSearchLabel')}">
  </cr-icon-button>
  <div id="iconsContainer">
    ${this.getItems_().map((item, index) => html`
      <cr-icon-button data-index="${index}"
          style="${this.getIconStyle_(item) || nothing}"
          @click="${this.onItemClick_}" title="${item.name || nothing}">
      </cr-icon-button>
    `)}
  </div>
</div>
<!--_html_template_end_-->`;
}

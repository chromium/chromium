// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html} from '//resources/lit/v3_0/lit.rollup.js';

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
      <!-- TODO(crbug.com/570552031): Replace placeholders with
           server-driven icons once AimEligibility icon config is wired up. -->
      <cr-icon-button id="showHistoryButton"
          iron-icon="composebox:notes-spark"
          @click="${this.onShowHistoryClick_}"
          aria-label="${this.i18n('aimThreadsHistoryLabel')}">
      </cr-icon-button>
      <cr-tooltip for="showHistoryButton" position="right" offset="16"
          animation-delay="0" hide-delay="0">
        ${this.i18n('aimThreadsHistoryLabel')}
      </cr-tooltip>
      <cr-icon-button id="newSearchButton"
          iron-icon="composebox:notes-spark"
          aria-label="${this.i18n('aimThreadsNewSearchLabel')}">
      </cr-icon-button>
      <cr-tooltip for="newSearchButton" position="right" offset="16"
          animation-delay="0" hide-delay="0">
        ${this.i18n('aimThreadsNewSearchLabel')}
      </cr-tooltip>
      <cr-icon-button id="tasksButton"
          iron-icon="composebox:notes-spark">
      </cr-icon-button>
    </div>
  </div>
  <!--_html_template_end_-->`;
  // clang-format on;
}

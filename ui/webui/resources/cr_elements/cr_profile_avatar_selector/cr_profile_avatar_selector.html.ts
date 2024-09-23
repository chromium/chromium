// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getImage} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrProfileAvatarSelectorElement} from './cr_profile_avatar_selector.js';

export function getHtml(this: CrProfileAvatarSelectorElement) {
  return html`
<cr-grid id="avatar-grid" role="radiogroup" columns="${this.columns}"
    focus-selector=".avatar"
    .ignoreModifiedKeyEvents="${this.ignoreModifiedKeyEvents}">
  ${this.avatars.map((item, index) => html`
    <!-- Wrapper div is needed so that only a single node is slotted in
        cr-grid for each avatar. -->
    <div>
      <div class="avatar-container ${this.getSelectedClass_(item)}">
        <cr-button class="avatar" role="radio" id="${this.getAvatarId_(index)}"
            data-index="${index}" aria-label="${item.label}"
            tabindex="${this.getTabIndex_(index, item)}"
            @click="${this.onAvatarClick_}"
            .style="background-image: ${getImage(item.url)}"
            aria-checked="${this.isAvatarSelected_(item)}">
        </cr-button>
        <cr-icon icon="cr:check" class="checkmark"></cr-icon>
      </div>
      <cr-tooltip for="${this.getAvatarId_(index)}" offset="0"
          fit-to-visible-bounds>
        ${item.label}
      </cr-tooltip>
    </div>
  `)}
</cr-grid>`;
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTooltipIconElement} from './cr_tooltip_icon.js';

export function getHtml(this: CrTooltipIconElement) {
  return html`
<cr-icon id="indicator" tabindex="0" aria-label="${this.iconAriaLabel}"
    aria-describedby="tooltip" icon="${this.iconClass}" role="img">
</cr-icon>
<cr-tooltip id="tooltip"
    for="indicator" position="${this.tooltipPosition}"
    fit-to-visible-bounds part="tooltip">
  <slot name="tooltip-text">${this.tooltipText}</slot>
</cr-tooltip>`;
}

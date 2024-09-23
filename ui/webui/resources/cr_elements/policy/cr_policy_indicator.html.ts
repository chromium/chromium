// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrPolicyIndicatorElement} from './cr_policy_indicator.js';

export function getHtml(this: CrPolicyIndicatorElement) {
  return html`
<cr-tooltip-icon ?hidden="${!this.getIndicatorVisible_()}"
    tooltip-text="${this.getIndicatorTooltip_()}"
    icon-class="${this.getIndicatorIcon_()}"
    icon-aria-label="${this.iconAriaLabel}">
</cr-tooltip-icon>`;
}

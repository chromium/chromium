// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrLinkRowElement} from './cr_link_row.js';

export function getHtml(this: CrLinkRowElement) {
  // clang-format off
  return html`
    ${this.startIcon ? html`
<cr-icon id="startIcon" .icon="${this.startIcon}" aria-hidden="true"></cr-icon>
    `: ''}
<div id="labelWrapper" ?hidden="${this.shouldHideLabelWrapper_()}">
  <div id="label" aria-hidden="${!this.ariaShowLabel}">
    ${this.label}
    <slot name="label"></slot>
  </div>
  <div id="subLabel" class="cr-secondary-text"
      aria-hidden="${!this.ariaShowSublabel}">
    ${this.subLabel}
    <slot name="sub-label"></slot>
  </div>
</div>
<slot></slot>
<div id="buttonAriaDescription" aria-hidden="true">
  ${this.getButtonAriaDescription_()}
</div>
<cr-icon-button id="icon" iron-icon="${this.getIcon_()}" role="link"
    part="icon" aria-roledescription="${this.roleDescription || nothing}"
    aria-describedby="buttonAriaDescription"
    aria-labelledby="label subLabel" ?disabled="${this.disabled}">
</cr-icon-button>`;
  // clang-format on
}

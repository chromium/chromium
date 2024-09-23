// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedFootnoteElement} from './managed_footnote.js';

export function getHtml(this: ManagedFootnoteElement) {
  return html`${
      this.isManaged_ ? html`
  <cr-icon .icon="${this.managedByIcon_}"></cr-icon>
  <div id="content" .innerHTML="${this.getManagementString_()}"></div>
` :
                        ''}`;
}

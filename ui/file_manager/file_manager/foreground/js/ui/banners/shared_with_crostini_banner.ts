// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType} from '../../../../common/js/volume_manager_types.js';

import {getTemplate} from './shared_with_crostini_banner.html.js';
import {StateBanner} from './state_banner.js';
import {BANNER_INFINITE_TIME} from './types.js';

export const TAG_NAME = 'shared-with-crostini-banner';

/** A banner shown when the current directory is shared with Crostini. */
export class SharedWithCrostiniBanner extends StateBanner {
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    return template.content.cloneNode(true);
  }

  override allowedVolumes() {
    return [
      {root: RootType.DOWNLOADS},
      {root: RootType.REMOVABLE},
      {root: RootType.ANDROID_FILES},
      {root: RootType.COMPUTERS_GRAND_ROOT},
      {root: RootType.COMPUTER},
      {root: RootType.DRIVE},
      {root: RootType.SHARED_DRIVES_GRAND_ROOT},
      {root: RootType.SHARED_DRIVE},
      {root: RootType.DRIVE_SHARED_WITH_ME},
      {root: RootType.CROSTINI},
      {root: RootType.ARCHIVE},
      {root: RootType.SMB},
    ];
  }

  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: SharedWithCrostiniBanner;
  }
}

customElements.define(TAG_NAME, SharedWithCrostiniBanner);

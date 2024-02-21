// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Menu} from './ui/menu.js';
import type {MultiMenuButton} from './ui/multi_menu_button.js';

export class SelectionMenuController {
  constructor(
      selectionMenuButton: MultiMenuButton, private readonly menu_: Menu) {
    selectionMenuButton.addEventListener(
        'menushow', this.onShowMenu_.bind(this));
    selectionMenuButton.addEventListener(
        'menuhide', this.onHideMenu_.bind(this));
  }

  private onShowMenu_() {
    this.menu_.classList.toggle('toolbar-menu', true);
    // crbug.com 752035 focus still on button, get rid of the tooltip
    document.querySelector('files-tooltip')?.hideTooltip();
  }

  private onHideMenu_() {
    // If menu is animating to close, then do not remove 'toolbar-menu' yet, it
    // will be removed at the end of FilesMenuItem.setMenuAsAnimating_ to avoid
    // flicker.  See crbug.com/862926.
    if (!this.menu_.classList.contains('animating')) {
      this.menu_.classList.toggle('toolbar-menu', false);
    }
  }
}

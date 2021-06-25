// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../elements/files_toggle_ripple.js';

import {Menu} from 'chrome://resources/js/cr/ui/menu.m.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';

import {FilesTooltip} from '../elements/files_tooltip.js';

import {MultiMenuButton} from './ui/multi_menu_button.js';

export class SelectionMenuController {
  /**
   * @param {!MultiMenuButton} selectionMenuButton
   * @param {!Menu} menu
   */
  constructor(selectionMenuButton, menu) {
    /**
     * @type {!FilesToggleRippleElement}
     * @const
     * @private
     */
    this.toggleRipple_ =
        /** @type {!FilesToggleRippleElement} */ (
            queryRequiredElement('files-toggle-ripple', selectionMenuButton));

    /**
     * @type {!Menu}
     * @const
     */
    this.menu_ = menu;

    selectionMenuButton.addEventListener(
        'menushow', this.onShowMenu_.bind(this));
    selectionMenuButton.addEventListener(
        'menuhide', this.onHideMenu_.bind(this));
  }

  /**
   * @private
   */
  onShowMenu_() {
    this.menu_.classList.toggle('toolbar-menu', true);
    this.toggleRipple_.activated = true;
    // crbug.com 752035 focus still on button, get rid of the tooltip
    /** @type {!FilesTooltip} */ (document.querySelector('files-tooltip'))
        .hideTooltip();
  }

  /**
   * @private
   */
  onHideMenu_() {
    // If menu is animating to close, then do not remove 'toolbar-menu' yet, it
    // will be removed at the end of FilesMenuItem.setMenuAsAnimating_ to avoid
    // flicker.  See crbug.com/862926.
    if (!this.menu_.classList.contains('animating')) {
      this.menu_.classList.toggle('toolbar-menu', false);
    }
    this.toggleRipple_.activated = false;
  }
}

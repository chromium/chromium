// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SelectionMenuController {
  /**
   * @param {!cr.ui.MenuButton} selectionMenuButton
   * @param {!cr.ui.Menu} menu
   */
  constructor(selectionMenuButton, menu) {
    /**
     * @type {!FilesToggleRipple}
     * @const
     * @private
     */
    this.toggleRipple_ =
        /** @type {!FilesToggleRipple} */ (
            queryRequiredElement('files-toggle-ripple', selectionMenuButton));

    /**
     * @type {!cr.ui.Menu}
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
    document.querySelector('files-tooltip').hideTooltip();
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

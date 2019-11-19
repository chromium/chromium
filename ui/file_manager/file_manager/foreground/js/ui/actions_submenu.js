// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class ActionsSubmenu {
  /** @param {!cr.ui.Menu} menu */
  constructor(menu) {
    /**
     * @private {!cr.ui.Menu}
     * @const
     */
    this.menu_ = menu;

    /**
     * @private {!cr.ui.MenuItem}
     * @const
     */
    this.separator_ = /** @type {!cr.ui.MenuItem} */
        (queryRequiredElement('#actions-separator', this.menu_));

    /**
     * @private {!Array<!cr.ui.MenuItem>}
     */
    this.items_ = [];
  }

  /**
   * @param {!Object} options
   * @return {cr.ui.MenuItem}
   * @private
   */
  addMenuItem_(options) {
    const menuItem = this.menu_.addMenuItem(options);
    menuItem.parentNode.insertBefore(menuItem, this.separator_);
    this.items_.push(menuItem);
    return menuItem;
  }

  /**
   * @param {ActionsModel} actionsModel
   * @param {Element} element The target element.
   */
  setActionsModel(actionsModel, element) {
    this.items_.forEach(item => {
      item.parentNode.removeChild(item);
    });
    this.items_ = [];

    const remainingActions = {};
    if (actionsModel) {
      const actions = actionsModel.getActions();
      Object.keys(actions).forEach(key => {
        remainingActions[key] = actions[key];
      });
    }

    // First add the sharing item (if available).
    const shareAction = remainingActions[ActionsModel.CommonActionId.SHARE];
    if (shareAction) {
      const menuItem = this.addMenuItem_({});
      menuItem.command = '#share';
      menuItem.classList.toggle('hide-on-toolbar', true);
      delete remainingActions[ActionsModel.CommonActionId.SHARE];
    }
    util.queryDecoratedElement('#share', cr.ui.Command)
        .canExecuteChange(element);

    // Then add the Manage in Drive item (if available).
    const manageInDriveAction =
        remainingActions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    if (manageInDriveAction) {
      const menuItem = this.addMenuItem_({});
      menuItem.command = '#manage-in-drive';
      menuItem.classList.toggle('hide-on-toolbar', true);
      delete remainingActions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    }
    util.queryDecoratedElement('#manage-in-drive', cr.ui.Command)
        .canExecuteChange(element);

    // Removing shortcuts is not rendered in the submenu to keep the previous
    // behavior. Shortcuts can be removed in the left nav using the roots menu.
    // TODO(mtomasz): Consider rendering the menu item here for consistency.
    util.queryDecoratedElement('#unpin-folder', cr.ui.Command)
        .canExecuteChange(element);

    // Both save-for-offline and offline-not-necessary are handled by the single
    // #toggle-pinned command.
    const saveForOfflineAction =
        remainingActions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
    const offlineNotNecessaryAction =
        remainingActions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
    if (saveForOfflineAction || offlineNotNecessaryAction) {
      const menuItem = this.addMenuItem_({});
      menuItem.command = '#toggle-pinned';
      if (saveForOfflineAction) {
        delete remainingActions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
      }
      if (offlineNotNecessaryAction) {
        delete remainingActions[ActionsModel.CommonActionId
                                    .OFFLINE_NOT_NECESSARY];
      }
    }
    util.queryDecoratedElement('#toggle-pinned', cr.ui.Command)
        .canExecuteChange(element);

    // Process all the rest as custom actions.
    Object.keys(remainingActions).forEach(key => {
      const action = remainingActions[key];
      const options = {label: action.getTitle()};
      const menuItem = this.addMenuItem_(options);

      menuItem.addEventListener('activate', () => {
        action.execute();
      });
    });

    this.separator_.hidden = !this.items_.length;
  }
}

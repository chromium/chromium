// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import type {Action, ActionsModel} from '../actions_model.js';
import {CommonActionId, InternalActionId} from '../actions_model.js';

import {Command} from './command.js';
import {FilesMenuItem} from './files_menu.js';
import type {Menu} from './menu.js';
import type {MenuItem} from './menu_item.js';

export class ActionsSubmenu {
  private readonly items_: MenuItem[] = [];
  private readonly separator_ =
      queryRequiredElement('#actions-separator', this.menu_) as MenuItem;
  constructor(private menu_: Menu) {}

  private addMenuItem_(label?: string): MenuItem {
    const menuItem = this.menu_.addMenuItem({label});
    crInjectTypeAndInit(menuItem, FilesMenuItem);
    menuItem.parentNode?.insertBefore(menuItem, this.separator_);
    this.items_.push(menuItem);
    return menuItem;
  }

  /**
   * @param element The target element.
   */
  setActionsModel(
      actionsModel: ActionsModel|null, element: Element|null = null) {
    this.items_.forEach((item: MenuItem) => {
      item.parentNode!.removeChild(item);
    });
    this.items_.length = 0;

    const remainingActions: Record<string, Action> = {};
    if (actionsModel) {
      const actions = actionsModel.getActions();
      for (const [key, action] of Object.entries(actions)) {
        remainingActions[key] = action!;
      }
    }

    // Then add the Manage in Drive item (if available).
    const manageInDriveAction =
        remainingActions[InternalActionId.MANAGE_IN_DRIVE];
    if (manageInDriveAction) {
      const menuItem = this.addMenuItem_();
      menuItem.command = '#manage-in-drive';
      menuItem.classList.toggle('hide-on-toolbar', true);
      delete remainingActions[InternalActionId.MANAGE_IN_DRIVE];
    }
    queryDecoratedElement('#manage-in-drive', Command)
        .canExecuteChange(element);

    // Removing shortcuts is not rendered in the submenu to keep the previous
    // behavior. Shortcuts can be removed in the left nav using the roots menu.
    // TODO(mtomasz): Consider rendering the menu item here for consistency.
    queryDecoratedElement('#unpin-folder', Command).canExecuteChange(element);

    // Both save-for-offline and offline-not-necessary are handled by the single
    // #toggle-pinned command.
    const saveForOfflineAction =
        remainingActions[CommonActionId.SAVE_FOR_OFFLINE];
    const offlineNotNecessaryAction =
        remainingActions[CommonActionId.OFFLINE_NOT_NECESSARY];
    if (saveForOfflineAction || offlineNotNecessaryAction) {
      const menuItem = this.addMenuItem_();
      menuItem.command = '#toggle-pinned';
      menuItem.classList.toggle('hide-on-toolbar', true);
      if (saveForOfflineAction) {
        delete remainingActions[CommonActionId.SAVE_FOR_OFFLINE];
      }
      if (offlineNotNecessaryAction) {
        delete remainingActions[CommonActionId.OFFLINE_NOT_NECESSARY];
      }
    }
    queryDecoratedElement('#toggle-pinned', Command).canExecuteChange(element);

    let hasCustomActions = false;
    // Process all the rest as custom actions.
    for (const action of Object.values(remainingActions)) {
      // If the action has no title it isn't visible to users, so we skip here.
      const label = action.getTitle();
      if (!label) {
        continue;
      }

      hasCustomActions = true;
      const menuItem = this.addMenuItem_(label);
      menuItem.addEventListener('activate', () => action.execute());
    }

    // All actions that are not custom actions are hide-on-toolbar, so
    // set hide-on-toolbar for the separator if there are no custom actions.
    this.separator_.classList.toggle('hide-on-toolbar', !hasCustomActions);

    this.separator_.hidden = !this.items_.length;
  }
}

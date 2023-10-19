// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import {ActionsModel} from '../actions_model.js';

import {Command} from './command.js';
import {FilesMenuItem} from './files_menu.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';

export class ActionsSubmenu {
  /** @param {!Menu} menu */
  constructor(menu) {
    /**
     * @private @type {!Menu}
     * @const
     */
    this.menu_ = menu;

    /**
     * @private @type {!MenuItem}
     * @const
     */
    this.separator_ = /** @type {!MenuItem} */
        // @ts-ignore: error TS2345: Argument of type 'Menu' is not assignable
        // to parameter of type 'Document | Element | HTMLElement |
        // DocumentFragment | undefined'.
        (queryRequiredElement('#actions-separator', this.menu_));

    /**
     * @private @type {!Array<!MenuItem>}
     */
    // @ts-ignore: error TS7008: Member 'items_' implicitly has an 'any[]' type.
    this.items_ = [];
  }

  /**
   * @param {!Object} options
   * @return {MenuItem}
   * @private
   */
  addMenuItem_(options) {
    // @ts-ignore: error TS2339: Property 'addMenuItem' does not exist on type
    // 'Menu'.
    const menuItem = this.menu_.addMenuItem(options);
    FilesMenuItem.decorate(menuItem);
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
      // @ts-ignore: error TS18047: 'item.parentNode' is possibly 'null'.
      item.parentNode.removeChild(item);
    });
    this.items_ = [];

    const remainingActions = {};
    if (actionsModel) {
      const actions = actionsModel.getActions();
      Object.keys(actions).forEach(key => {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        remainingActions[key] = actions[key];
      });
    }

    // First add the sharing item (if available).
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    const shareAction = remainingActions[ActionsModel.CommonActionId.SHARE];
    if (shareAction) {
      const menuItem = this.addMenuItem_({});
      // @ts-ignore: error TS2339: Property 'command' does not exist on type
      // 'MenuItem'.
      menuItem.command = '#share';
      menuItem.classList.toggle('hide-on-toolbar', true);
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      delete remainingActions[ActionsModel.CommonActionId.SHARE];
    }
    queryDecoratedElement('#share', Command).canExecuteChange(element);

    // Then add the Manage in Drive item (if available).
    const manageInDriveAction =
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        remainingActions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    if (manageInDriveAction) {
      const menuItem = this.addMenuItem_({});
      // @ts-ignore: error TS2339: Property 'command' does not exist on type
      // 'MenuItem'.
      menuItem.command = '#manage-in-drive';
      menuItem.classList.toggle('hide-on-toolbar', true);
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      delete remainingActions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
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
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        remainingActions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
    const offlineNotNecessaryAction =
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        remainingActions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
    if (saveForOfflineAction || offlineNotNecessaryAction) {
      const menuItem = this.addMenuItem_({});
      // @ts-ignore: error TS2339: Property 'command' does not exist on type
      // 'MenuItem'.
      menuItem.command = '#toggle-pinned';
      menuItem.classList.toggle('hide-on-toolbar', true);
      if (saveForOfflineAction) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        delete remainingActions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
      }
      if (offlineNotNecessaryAction) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        delete remainingActions[ActionsModel.CommonActionId
                                    .OFFLINE_NOT_NECESSARY];
      }
    }
    queryDecoratedElement('#toggle-pinned', Command).canExecuteChange(element);

    let hasCustomActions = false;
    // Process all the rest as custom actions.
    Object.keys(remainingActions).forEach(key => {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      const action = remainingActions[key];
      // If the action has no title it isn't visible to users, so we skip here.
      if (!action.getTitle()) {
        return;
      }
      hasCustomActions = true;
      const options = {label: action.getTitle()};
      const menuItem = this.addMenuItem_(options);

      menuItem.addEventListener('activate', () => {
        action.execute();
      });
    });

    // All actions that are not custom actions are hide-on-toolbar, so
    // set hide-on-toolbar for the separator if there are no custom actions.
    this.separator_.classList.toggle('hide-on-toolbar', !hasCustomActions);

    this.separator_.hidden = !this.items_.length;
  }
}

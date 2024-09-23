// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {iconSetToCSSBackgroundImageValue} from '../../../common/js/util.js';
import type {ProvidersModel} from '../providers_model.js';

import {FilesMenuItem} from './files_menu.js';
import type {Menu} from './menu.js';

/** Fills out the menu for mounting or installing new providers. */
export class ProvidersMenu {
  constructor(
      private readonly model_: ProvidersModel, private readonly menu_: Menu) {
    this.menu_.addEventListener('update', this.onUpdate_.bind(this));
  }

  private clearProviders_() {
    while (this.menu_.lastChild) {
      this.menu_.removeChild(this.menu_.lastChild);
    }
  }

  private addMenuItem_(): FilesMenuItem {
    const menuItem = this.menu_.addMenuItem({});
    crInjectTypeAndInit(menuItem, FilesMenuItem);
    return menuItem as FilesMenuItem;
  }

  /**
   * @param providerId ID of the provider.
   * @param iconSet Set of icons for the provider.
   * @param name Already localized name of the provider.
   */
  private addProvider_(
      providerId: string, iconSet: chrome.fileManagerPrivate.IconSet,
      name: string) {
    const item = this.addMenuItem_();
    item.label = name;

    const iconImage = iconSetToCSSBackgroundImageValue(iconSet);
    if (iconImage === 'none' && providerId === '@smb') {
      item.iconStartFileType = 'smb';
    } else {
      item.iconStartImage = iconImage;
    }

    item.addEventListener(
        'activate', this.onItemActivate_.bind(this, providerId));
  }

  private onUpdate_(_event: Event) {
    this.model_.getMountableProviders().then(providers => {
      this.clearProviders_();
      providers.forEach(provider => {
        this.addProvider_(provider.providerId, provider.iconSet, provider.name);
      });
    });
  }

  private onItemActivate_(providerId: string, _event: Event) {
    this.model_.requestMount(providerId);
  }

  /**
   *  Sends an update event to the sub menu to trigger a reload of its content.
   */
  updateSubMenu() {
    this.menu_.dispatchEvent(new Event('update'));
  }
}

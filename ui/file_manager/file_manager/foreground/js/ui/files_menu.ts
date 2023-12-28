// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.js';
import type {PaperRippleElement} from 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {Menu} from './menu.js';
import {MenuItem, type MenuItemActivationEvent} from './menu_item.js';

/**
 * Menu item with ripple animation.
 */
export class FilesMenuItem extends MenuItem {
  private animating_: boolean = false;
  private hidden_: boolean|undefined = undefined;
  private label_: HTMLElement;
  private iconStart_: HTMLElement;
  private iconManaged_: HTMLElement;
  private iconEnd_: HTMLElement;
  private ripple_: PaperRippleElement;

  descriptor: chrome.fileManagerPrivate.FileTaskDescriptor|null = null;

  constructor() {
    super();
    throw new Error('Designed to decorate elements');
  }

  override initialize() {
    this.animating_ = false;

    // Custom menu item can have sophisticated content (elements).
    if (!this.children.length) {
      this.label_ = document.createElement('span');
      this.label_.textContent = this.textContent;

      this.iconStart_ = document.createElement('div');
      this.iconStart_.classList.add('icon', 'start');

      this.iconManaged_ = document.createElement('div');
      this.iconManaged_.classList.add('icon', 'managed');

      this.iconEnd_ = document.createElement('div');
      this.iconEnd_.classList.add('icon', 'end');
      /**
       * This is hidden by default because most of the menu items require
       * neither the end icon nor the managed icon, so the component that
       * plans to use either end icon should explicitly make it visible.
       */
      this.setIconEndHidden(true);
      this.toggleManagedIcon(/*visible=*/ false);

      // Override with standard menu item elements.
      this.textContent = '';
      this.appendChild(this.iconStart_);
      this.appendChild(this.label_);
      this.appendChild(this.iconManaged_);
      this.appendChild(this.iconEnd_);
    }

    this.ripple_ = document.createElement('paper-ripple');
    this.appendChild(this.ripple_);

    this.addEventListener('activate', this.onActivated_.bind(this));
  }

  /**
   * Handles activate event.
   */
  private onActivated_(evt: Event) {
    const event = evt as MenuItemActivationEvent;
    // Perform ripple animation if it's activated by keyboard.
    if (event.originalEvent instanceof KeyboardEvent) {
      this.ripple_.simulatedRipple();
    }

    // Perform fade out animation.
    const menu = this.parentNode;
    assertInstanceof(menu, Menu);
    // If activation was on a menu-item that hosts a sub-menu, don't animate
    const subMenuId = (event.target as MenuItem).getAttribute('sub-menu');
    if (subMenuId) {
      if (document.querySelector(subMenuId) !== null) {
        return;
      }
    }
    this.setMenuAsAnimating_(menu, /*animating=*/ true);

    const player = menu.animate(
        [
          {
            opacity: 1,
            offset: 0,
          },
          {
            opacity: 0,
            offset: 1,
          },
        ],
        300);

    player.addEventListener(
        'finish',
        this.setMenuAsAnimating_.bind(this, menu, /*animating=*/ false));
  }

  /**
   * Sets menu as animating. Pass value equal to true to set it as animating.
   */
  private setMenuAsAnimating_(menu: Menu, value: boolean) {
    menu.classList.toggle('animating', value);

    for (let i = 0; i < menu.menuItems.length; i++) {
      const menuItem = menu.menuItems[i];
      if (menuItem instanceof FilesMenuItem) {
        menuItem.setAnimating_(value);
      }
    }

    if (!value) {
      menu.classList.remove('toolbar-menu');
    }
  }

  /**
   * Sets the menu item as animating. Pass value set to true to set this as
   * animating.
   */
  private setAnimating_(value: boolean) {
    this.animating_ = value;

    if (this.animating_) {
      return;
    }

    // Update hidden property if there is a pending change.
    if (this.hidden_ !== undefined) {
      this.hidden = this.hidden_;
      this.hidden_ = undefined;
    }
  }

  override get hidden(): boolean {
    if (this.hidden_ !== undefined) {
      return this.hidden_;
    }

    return Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        ?.get?.call(this);
  }

  /**
   * Overrides hidden property to block the change of hidden property while
   * menu is animating.
   */
  override set hidden(value: boolean) {
    if (this.animating_) {
      this.hidden_ = value;
      return;
    }

    Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'hidden')
        ?.set?.call(this, value);
  }

  override get label(): string {
    return this.label_.textContent || '';
  }

  override set label(value: string) {
    this.label_.textContent = value;
  }

  get iconStartImage(): string {
    return this.iconStart_.style.backgroundImage;
  }

  set iconStartImage(value: string) {
    this.iconStart_.setAttribute('style', 'background-image: ' + value);
  }

  get iconStartFileType(): string {
    return this.iconStart_.getAttribute('file-type-icon') || '';
  }

  set iconStartFileType(value: string) {
    this.iconStart_.setAttribute('file-type-icon', value);
  }

  /**
   * Sets or removes the `is-managed` attribute.
   */
  toggleIsManagedAttribute(isManaged: boolean) {
    this.toggleAttribute('is-managed', isManaged);
  }

  /**
   * Sets the `is-default` attribute.
   */
  setIsDefaultAttribute() {
    this.toggleAttribute('is-default', true);
  }

  /**
   * Toggles visibility of the `Managed by Policy` icon.
   */
  toggleManagedIcon(visible: boolean) {
    this.iconManaged_.toggleAttribute('hidden', !visible);
    this.toggleIsManagedAttribute(visible);
  }

  get iconEndImage(): string {
    return this.iconEnd_.style.backgroundImage;
  }

  set iconEndImage(value: string) {
    this.iconEnd_.setAttribute('style', 'background-image: ' + value);
  }

  get iconEndFileType(): string {
    return this.iconEnd_.getAttribute('file-type-icon') || '';
  }

  set iconEndFileType(value: string) {
    this.iconEnd_.setAttribute('file-type-icon', value);
  }

  removeIconEndFileType() {
    this.iconEnd_.removeAttribute('file-type-icon');
  }

  setIconEndHidden(isHidden: boolean) {
    this.iconEnd_.toggleAttribute('hidden', isHidden);
  }
}


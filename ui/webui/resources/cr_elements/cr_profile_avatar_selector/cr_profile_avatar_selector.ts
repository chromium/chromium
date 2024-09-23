// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector' is an element that displays
 * profile avatar icons and allows an avatar to be selected.
 */

import '../cr_button/cr_button.js';
import '../cr_grid/cr_grid.js';
import '../cr_icon/cr_icon.js';
import '../cr_tooltip/cr_tooltip.js';
import '../icons_lit.html.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_profile_avatar_selector.css.js';
import {getHtml} from './cr_profile_avatar_selector.html.js';

export interface AvatarIcon {
  url: string;
  label: string;
  index: number;
  isGaiaAvatar: boolean;
  selected: boolean;
}

export class CrProfileAvatarSelectorElement extends CrLitElement {
  static get is() {
    return 'cr-profile-avatar-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The list of profile avatar URLs and labels.
       */
      avatars: {type: Array},

      /**
       * The currently selected profile avatar icon, if any.
       */
      selectedAvatar: {
        type: Object,
        notify: true,
      },

      ignoreModifiedKeyEvents: {type: Boolean},

      /**
       * The currently selected profile avatar icon index, or '-1' if none is
       * selected.
       */
      tabFocusableAvatar_: {type: Number},

      /**
       * Number of columns in the grid.
       */
      columns: {type: Number},
    };
  }

  avatars: AvatarIcon[] = [];
  selectedAvatar: AvatarIcon|null = null;
  ignoreModifiedKeyEvents: boolean = false;
  columns: number = 6;
  private tabFocusableAvatar_: number = -1;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('avatars') ||
        changedProperties.has('selectedAvatar')) {
      const selectedAvatar =
          this.avatars.find(avatar => this.isAvatarSelected_(avatar));
      this.tabFocusableAvatar_ = selectedAvatar ? selectedAvatar.index : -1;
    }
  }

  protected getAvatarId_(index: number): string {
    return 'avatarId' + index;
  }

  protected getTabIndex_(index: number, item: AvatarIcon): string {
    if (item.index === this.tabFocusableAvatar_) {
      return '0';
    }

    // If no avatar is selected, focus the first element of the grid on 'tab'.
    if (this.tabFocusableAvatar_ === -1 && index === 0) {
      return '0';
    }
    return '-1';
  }

  protected getSelectedClass_(avatarItem: AvatarIcon): string {
    // TODO(dpapad): Rename 'iron-selected' to 'selected' now that this CSS
    // class is not assigned by any iron-* behavior.
    return this.isAvatarSelected_(avatarItem) ? 'iron-selected' : '';
  }

  protected isAvatarSelected_(avatarItem: AvatarIcon): boolean {
    return avatarItem.selected ||
        (!!this.selectedAvatar &&
         this.selectedAvatar.index === avatarItem.index);
  }

  protected onAvatarClick_(e: Event) {
    // |selectedAvatar| is set to pass back selection to the owner of this
    // component.
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    this.selectedAvatar = this.avatars[index]!;

    // Autoscroll to selected avatar if it is not completely visible.
    const avatarList =
        this.shadowRoot!.querySelectorAll<HTMLElement>('.avatar-container');
    assert(avatarList.length > 0);
    const selectedAvatarElement = avatarList[index];
    selectedAvatarElement!.scrollIntoViewIfNeeded();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-profile-avatar-selector': CrProfileAvatarSelectorElement;
  }
}

customElements.define(
    CrProfileAvatarSelectorElement.is, CrProfileAvatarSelectorElement);

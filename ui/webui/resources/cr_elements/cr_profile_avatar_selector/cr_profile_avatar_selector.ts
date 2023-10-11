// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector' is an element that displays
 * profile avatar icons and allows an avatar to be selected.
 */

import '../cr_button/cr_button.js';
import '../cr_shared_vars.css.js';
import '../cr_shared_style.css.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './cr_profile_avatar_selector_grid.js';

import {assert} from '//resources/js/assert.js';
import {getImage} from '//resources/js/icon.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_profile_avatar_selector.html.js';

export interface AvatarIcon {
  url: string;
  label: string;
  index: number;
  isGaiaAvatar: boolean;
  selected: boolean;
}

export class CrProfileAvatarSelectorElement extends PolymerElement {
  static get is() {
    return 'cr-profile-avatar-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The list of profile avatar URLs and labels.
       */
      avatars: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The currently selected profile avatar icon, if any.
       */
      selectedAvatar: {
        type: Object,
        notify: true,
      },

      ignoreModifiedKeyEvents: {
        type: Boolean,
        value: false,
      },

      /**
       * The currently selected profile avatar icon index, or '-1' if none is
       * selected.
       */
      tabFocusableAvatar_: {
        type: Number,
        computed: 'computeTabFocusableAvatar_(avatars, selectedAvatar)',
      },
    };
  }

  avatars: AvatarIcon[];
  selectedAvatar: AvatarIcon|null;
  ignoreModifiedKeyEvents: boolean;
  private tabFocusableAvatar_: number;

  private getAvatarId_(index: number): string {
    return 'avatarId' + index;
  }

  private getTabIndex_(index: number, item: AvatarIcon): string {
    if (item.index === this.tabFocusableAvatar_) {
      return '0';
    }

    // If no avatar is selected, focus the first element of the grid on 'tab'.
    if (this.tabFocusableAvatar_ === -1 && index === 0) {
      return '0';
    }
    return '-1';
  }

  private computeTabFocusableAvatar_(): number {
    const selectedAvatar =
        this.avatars.find(avatar => this.isAvatarSelected(avatar));
    return selectedAvatar ? selectedAvatar.index : -1;
  }

  private getSelectedClass_(avatarItem: AvatarIcon): string {
    // TODO(dpapad): Rename 'iron-selected' to 'selected' now that this CSS
    // class is not assigned by any iron-* behavior.
    return this.isAvatarSelected(avatarItem) ? 'iron-selected' : '';
  }

  private getCheckedAttribute_(avatarItem: AvatarIcon): string {
    return this.isAvatarSelected(avatarItem) ? 'true' : 'false';
  }

  private isAvatarSelected(avatarItem: AvatarIcon): boolean {
    return !!avatarItem &&
        (avatarItem.selected ||
         (!!this.selectedAvatar &&
          this.selectedAvatar.index === avatarItem.index));
  }

  /**
   * @return A CSS image-set for multiple scale factors.
   */
  private getIconImageSet_(iconUrl: string): string {
    return getImage(iconUrl);
  }

  private onAvatarClick_(e: DomRepeatEvent<AvatarIcon>) {
    // |selectedAvatar| is set to pass back selection to the owner of this
    // component.
    this.selectedAvatar = e.model.item;

    // Autoscroll to selected avatar if it is not completely visible.
    const avatarList =
        this.shadowRoot!.querySelectorAll<HTMLElement>('.avatar-container');
    assert(avatarList.length > 0);
    const selectedAvatarElement = avatarList[e.model.index];
    assert(selectedAvatarElement!.classList.contains('iron-selected'));
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

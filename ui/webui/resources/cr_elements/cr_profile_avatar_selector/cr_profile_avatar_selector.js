// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector' is an element that displays
 * profile avatar icons and allows an avatar to be selected.
 */

/**
 * @typedef {{url: string,
 *            label: string,
 *            index: (number),
 *            isGaiaAvatar: (boolean),
 *            selected: (boolean)}}
 */
/* #export */ let AvatarIcon;

Polymer({
  is: 'cr-profile-avatar-selector',

  properties: {
    /**
     * The list of profile avatar URLs and labels.
     * @type {!Array<!AvatarIcon>}
     */
    avatars: {
      type: Array,
      value() {
        return [];
      }
    },

    /**
     * The currently selected profile avatar icon, if any.
     * @type {?AvatarIcon}
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
     * @type {number}
     */
    tabFocusableAvatar_: {
      type: Number,
      computed: 'computeTabFocusableAvatar_(avatars, selectedAvatar)',
    },
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getAvatarId_(index) {
    return 'avatarId' + index;
  },

  /**
   * @param {number} index
   * @param {!AvatarIcon} item
   * @return {string}
   * @private
   */
  getTabIndex_(index, item) {
    if (item.index === this.tabFocusableAvatar_) {
      return '0';
    }

    // If no avatar is selected, focus the first element of the grid on 'tab'.
    if (this.tabFocusableAvatar_ === -1 && index === 0) {
      return '0';
    }
    return '-1';
  },

  /**
   * @return {number}
   * @private
   */
  computeTabFocusableAvatar_() {
    const selectedAvatar =
        this.avatars.find(avatar => this.isAvatarSelected(avatar));
    return selectedAvatar ? selectedAvatar.index : -1;
  },

  /** @private */
  getSelectedClass_(avatarItem) {
    // TODO(dpapad): Rename 'iron-selected' to 'selected' now that this CSS
    // class is not assigned by any iron-* behavior.
    return this.isAvatarSelected(avatarItem) ? 'iron-selected' : '';
  },

  /**
   * @param {AvatarIcon} avatarItem
   * @return {string}
   * @private
   */
  getCheckedAttribute_(avatarItem) {
    return this.isAvatarSelected(avatarItem) ? 'true' : 'false';
  },

  /**
   * @param {AvatarIcon} avatarItem
   * @return {boolean}
   * @private
   */
  isAvatarSelected(avatarItem) {
    return !!avatarItem &&
        (avatarItem.selected ||
         (!!this.selectedAvatar &&
          this.selectedAvatar.index === avatarItem.index));
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAvatarTap_(e) {
    // |selectedAvatar| is set to pass back selection to the owner of this
    // component.
    this.selectedAvatar =
        /** @type {!{model: {item: !AvatarIcon}}} */ (e).model.item;
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');

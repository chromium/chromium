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

    /** @private {?HTMLElement} */
    selectedAvatarElement_: Object,

    ignoreModifiedKeyEvents: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  getSelectedClass_(isSelected) {
    // TODO(dpapad): Rename 'iron-selected' to 'selected' now that this CSS
    // class is not assigned by any iron-* behavior.
    return isSelected ? 'iron-selected' : '';
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
    // Manual selection for profile creation
    if (this.selectedAvatarElement_) {
      this.selectedAvatarElement_.classList.remove('iron-selected');
    }
    this.selectedAvatarElement_ = /** @type {!HTMLElement} */ (e.target);
    this.selectedAvatarElement_.classList.add('iron-selected');

    // |selectedAvatar| is set to pass back selection to the owner of this
    // component.
    this.selectedAvatar =
        /** @type {!{model: {item: !AvatarIcon}}} */ (e).model.item;
  },
});

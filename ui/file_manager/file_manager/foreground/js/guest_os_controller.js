// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {listMountableGuests} from '../../common/js/api.js';
import {GuestOsPlaceholder} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';

import {DirectoryModel} from './directory_model.js';
import {NavigationModelFakeItem, NavigationModelItemType} from './navigation_list_model.js';
import {DirectoryTree} from './ui/directory_tree.js';


/**
 * GuestOsController handles the foreground UI relating to Guest OSs.
 */
export class GuestOsController {
  /**
   * @param {!DirectoryModel} directoryModel DirectoryModel.
   * @param {!DirectoryTree} directoryTree DirectoryTree.
   * @param {boolean} disabled Whether the Guest OS item should be disabled.
   *     Defaults to false.
   */
  constructor(directoryModel, directoryTree, disabled = false) {
    if (!util.isGuestOsEnabled()) {
      console.warn('Created a guest os controller when it\'s not enabled');
    }
    /** @private @const */
    this.directoryModel_ = directoryModel;

    /** @private @const */
    this.directoryTree_ = directoryTree;

    /** @private @const {boolean} */
    this.disabled_ = disabled;

    chrome.fileManagerPrivate.onMountableGuestsChanged.addListener(
        this.onMountableGuestsChanged.bind(this));
  }

  /**
   * Refresh the Guest OS placeholders by fetching an updated list of guests,
   * adding them to the directory tree and triggering a redraw.
   */
  async refresh() {
    const guests = await listMountableGuests();
    this.onMountableGuestsChanged(guests);
  }

  /**
   * Updates the list of Guest OSs when we receive an event for the list of
   * registered guests changing, by adding them to the directory tree and
   * triggering a redraw.
   * @param {!Array<!chrome.fileManagerPrivate.MountableGuest>} guests
   */
  async onMountableGuestsChanged(guests) {
    this.directoryTree_.dataModel.guestOsPlaceholders = guests.map(guest => {
      const navigationModelItem = new NavigationModelFakeItem(
          guest.displayName, NavigationModelItemType.GUEST_OS,
          new GuestOsPlaceholder(guest.displayName, guest.id, guest.vmType));
      navigationModelItem.disabled = this.disabled_;
      return navigationModelItem;
    });

    // Redraw the tree to ensure any newly added/removed roots are updated.
    this.directoryTree_.redraw(false);
  }
}

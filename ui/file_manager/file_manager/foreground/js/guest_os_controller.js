// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {listMountableGuests} from '../../common/js/api.js';
import {GuestOsPlaceholder} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {addUiEntry, removeUiEntry} from '../../state/actions/ui_entries.js';
import {getEntry, getStore} from '../../state/store.js';

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
   * @param {!VolumeManager} volumeManager VolumeManager.
   */
  constructor(directoryModel, directoryTree, volumeManager) {
    if (!util.isGuestOsEnabled()) {
      console.warn('Created a guest os controller when it\'s not enabled');
    }
    /** @private @const */
    this.directoryModel_ = directoryModel;

    /** @private @const */
    this.directoryTree_ = directoryTree;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

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
    const store = getStore();
    const newGuestIdSet = new Set(guests.map(guest => guest.id));
    const state = store.getState();
    // Remove non-existing guest os.
    for (const uiEntryKey of state.uiEntries) {
      const uiEntry = getEntry(state, uiEntryKey);
      if (uiEntry && 'guest_id' in uiEntry &&
          !newGuestIdSet.has(uiEntry.guest_id)) {
        store.dispatch(removeUiEntry({key: uiEntryKey}));
      }
    }

    const newGuestOsPlaceholders =
        guests.map(guest => {
          const guestOsEntry =
              new GuestOsPlaceholder(guest.displayName, guest.id, guest.vmType);
          const navigationModelItem = new NavigationModelFakeItem(
              guest.displayName, NavigationModelItemType.GUEST_OS,
              guestOsEntry);
          const volumeType =
              guest.vmType == chrome.fileManagerPrivate.VmType.ARCVM ?
              VolumeManagerCommon.VolumeType.ANDROID_FILES :
              VolumeManagerCommon.VolumeType.GUEST_OS;

          navigationModelItem.disabled =
              this.volumeManager_.isDisabled(volumeType);
          store.dispatch(addUiEntry({entry: guestOsEntry}));
          return navigationModelItem;
        });

    if (!util.isFilesAppExperimental()) {
      this.directoryTree_.dataModel.guestOsPlaceholders =
          newGuestOsPlaceholders;
      // Redraw the tree to ensure any newly added/removed roots are
      // updated.
      this.directoryTree_.redraw(false);
    }
  }
}

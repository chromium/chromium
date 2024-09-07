// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {listMountableGuests} from '../../common/js/api.js';
import {GuestOsPlaceholder} from '../../common/js/files_app_entry_types.js';
import {isGuestOsEnabled} from '../../common/js/flags.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {getEntry, getStore} from '../../state/store.js';

/**
 * GuestOsController handles the foreground UI relating to Guest OSs.
 */
export class GuestOsController {
  constructor() {
    if (!isGuestOsEnabled()) {
      console.warn('Created a guest os controller when it\'s not enabled');
    }

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
   */
  async onMountableGuestsChanged(
      guests: chrome.fileManagerPrivate.MountableGuest[]) {
    const store = getStore();
    const newGuestIdSet = new Set(guests.map(guest => guest.id));
    const state = store.getState();
    // Remove non-existing guest os.
    for (const uiEntryKey of state.uiEntries) {
      const uiEntry = getEntry(state, uiEntryKey);
      if (uiEntry && 'guest_id' in uiEntry &&
          !newGuestIdSet.has((uiEntry as GuestOsPlaceholder).guest_id)) {
        store.dispatch(removeUiEntry(uiEntryKey));
      }
    }

    guests.forEach(guest => {
      const guestOsEntry =
          new GuestOsPlaceholder(guest.displayName, guest.id, guest.vmType);
      store.dispatch(addUiEntry(guestOsEntry));
    });
  }
}

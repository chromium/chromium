// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isFakeEntry, unwrapEntry} from '../../common/js/entry_utils.js';
import {updateMetadata} from '../../state/ducks/all_entries.js';
import {getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {FileMetadataFormatter} from './ui/file_metadata_formatter.js';
import {ListContainer} from './ui/list_container.js';

/**
 * Controller for list contents update.
 */
export class MetadataUpdateController {
  /**
   * @param {!ListContainer} listContainer
   * @param {!DirectoryModel} directoryModel
   * @param {!MetadataModel} metadataModel
   * @param {!FileMetadataFormatter} fileMetadataFormatter
   */
  constructor(
      listContainer, directoryModel, metadataModel, fileMetadataFormatter) {
    /** @private @const @type {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const @type {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const @type {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const @type {!FileMetadataFormatter} */
    this.fileMetadataFormatter_ = fileMetadataFormatter;

    // @ts-ignore: error TS2304: Cannot find name 'Store'.
    /** @private @type {!Store} */
    this.store_ = getStore();

    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
    metadataModel.addEventListener(
        'update', this.onCachedMetadataUpdate_.bind(this));

    // Update metadata to change 'Today' and 'Yesterday' dates.
    const today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this),
        today.getTime() + MetadataUpdateController.MILLISECONDS_IN_DAY_ -
            Date.now() + 1000);
  }

  /**
   * Clears metadata cache for the current directory and its decendents.
   */
  refreshCurrentDirectoryMetadata() {
    const entries = this.directoryModel_.getFileList().slice();
    const directoryEntry = this.directoryModel_.getCurrentDirEntry();
    if (!directoryEntry) {
      return;
    }

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    const changedEntries = (isFakeEntry(directoryEntry) ? [] : [
                             unwrapEntry(directoryEntry),
                           ]).concat(entries);
    // @ts-ignore: error TS2345: Argument of type '(FileSystemEntry |
    // FilesAppEntry)[]' is not assignable to parameter of type
    // 'FileSystemEntry[]'.
    this.metadataModel_.notifyEntriesChanged(changedEntries);

    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.
    this.metadataModel_.get(
        // @ts-ignore: error TS2345: Argument of type '(FileSystemEntry |
        // FilesAppEntry)[]' is not assignable to parameter of type
        // 'FileSystemEntry[]'.
        changedEntries, this.directoryModel_.getPrefetchPropertyNames());
  }

  /**
   * Handles local metadata changes in the currect directory.
   * @param {Event} event Change event.
   * @private
   */
  onCachedMetadataUpdate_(event) {
    // @ts-ignore: error TS2339: Property 'entries' does not exist on type
    // 'Event'.
    this.updateStore_(event.entries);
    this.listContainer_.dataModel?.refreshGroupBySnapshot();
    // TODO(hirono): Specify property name instead of metadata type.
    this.listContainer_.currentView.updateListItemsMetadata(
        // @ts-ignore: error TS2339: Property 'entries' does not exist on type
        // 'Event'.
        'filesystem', event.entries);
    this.listContainer_.currentView.updateListItemsMetadata(
        // @ts-ignore: error TS2339: Property 'entries' does not exist on type
        // 'Event'.
        'external', event.entries);
  }

  /**
   * @private
   */
  dailyUpdateModificationTime_() {
    const entries = /** @type {!Array<!Entry>} */ (
        this.directoryModel_.getFileList().slice());
    this.metadataModel_.get(entries, ['modificationTime']).then(() => {
      this.listContainer_.currentView.updateListItemsMetadata(
          'filesystem', entries);
    });
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this),
        MetadataUpdateController.MILLISECONDS_IN_DAY_);
  }

  /**
   * @private
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      const use12hourClock = !prefs.use24hourClock;
      this.fileMetadataFormatter_.setDateTimeFormat(use12hourClock);
      // TODO(oka): Remove these two lines, and add fileMetadataFormatter to
      // constructor for each field instead.
      this.listContainer_.table.setDateTimeFormat(use12hourClock);
      this.refreshCurrentDirectoryMetadata();
    });
  }

  /**
   * Sends the new metadata to the Store.
   * @private
   */
  // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any' type.
  updateStore_(entries) {
    const metadata = entries.map(
        // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
        e => ({
          entry: e,
          metadata: this.metadataModel_.getCache(
              [e], this.directoryModel_.getPrefetchPropertyNames())[0],
        }));

    this.store_.dispatch(updateMetadata({metadata}));
  }
}

/**
 * Number of milliseconds in a day.
 * @private @const @type {number}
 */
// @ts-ignore: error TS2341: Property 'MILLISECONDS_IN_DAY_' is private and only
// accessible within class 'MetadataUpdateController'.
MetadataUpdateController.MILLISECONDS_IN_DAY_ = 24 * 60 * 60 * 1000;

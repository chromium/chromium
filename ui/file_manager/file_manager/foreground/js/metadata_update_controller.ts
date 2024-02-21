// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isFakeEntry, unwrapEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {type EntryMetadata, updateMetadata} from '../../state/ducks/all_entries.js';
import {getStore} from '../../state/store.js';

import type {DirectoryModel} from './directory_model.js';
import type {MetadataSetEvent} from './metadata/metadata_cache_set.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {FileMetadataFormatter} from './ui/file_metadata_formatter.js';
import type {ListContainer} from './ui/list_container.js';


/** Number of milliseconds in a day. */
const MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

/** Controller for list contents update. */
export class MetadataUpdateController {
  private readonly store_ = getStore();

  constructor(
      private readonly listContainer_: ListContainer,
      private readonly directoryModel_: DirectoryModel,
      private readonly metadataModel_: MetadataModel,
      private readonly fileMetadataFormatter_: FileMetadataFormatter) {
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
    this.metadataModel_.addEventListener(
        'update', this.onCachedMetadataUpdate_.bind(this));

    // Update metadata to change 'Today' and 'Yesterday' dates.
    const today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this),
        today.getTime() + MILLISECONDS_IN_DAY - Date.now() + 1000);
  }

  /** Clears metadata cache for the current directory and its descendants. */
  refreshCurrentDirectoryMetadata() {
    const entries = this.directoryModel_.getFileList().slice();
    const directoryEntry = this.directoryModel_.getCurrentDirEntry();
    if (!directoryEntry) {
      return;
    }

    const changedEntries =
        (isFakeEntry(directoryEntry) ? [] : [
          unwrapEntry(directoryEntry) as (Entry | FilesAppEntry),
        ]).concat(entries);
    this.metadataModel_.notifyEntriesChanged(changedEntries);

    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.
    this.metadataModel_.get(
        changedEntries, this.directoryModel_.getPrefetchPropertyNames());
  }

  /**
   * Handles local metadata changes in the current directory.
   * @param event Change event.
   */
  private onCachedMetadataUpdate_(event: MetadataSetEvent) {
    this.updateStore_(event.entries);
    this.listContainer_.dataModel?.refreshGroupBySnapshot();
    // TODO(hirono): Specify property name instead of metadata type.
    this.listContainer_.currentView.updateListItemsMetadata(
        'filesystem', event.entries);
    this.listContainer_.currentView.updateListItemsMetadata(
        'external', event.entries);
  }

  private dailyUpdateModificationTime_() {
    const entries = this.directoryModel_.getFileList().slice() as Entry[];
    this.metadataModel_.get(entries, ['modificationTime']).then(() => {
      this.listContainer_.currentView.updateListItemsMetadata(
          'filesystem', entries);
    });
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this), MILLISECONDS_IN_DAY);
  }

  private onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      const use12hourClock = !prefs.use24hourClock;
      this.fileMetadataFormatter_.setDateTimeFormat(use12hourClock);
      // TODO(oka): Remove these two lines, and add fileMetadataFormatter to
      // constructor for each field instead.
      this.listContainer_.table.setDateTimeFormat(use12hourClock);
      this.refreshCurrentDirectoryMetadata();
    });
  }

  /** Sends the new metadata to the Store. */
  private updateStore_(entries: Array<Entry|FilesAppEntry>) {
    const metadata: EntryMetadata[] = entries.map(
        (e: Entry|FilesAppEntry): EntryMetadata => ({
          entry: e,
          metadata: this.metadataModel_.getCache(
              [e], this.directoryModel_.getPrefetchPropertyNames())[0]!,
        }));

    this.store_.dispatch(updateMetadata({metadata}));
  }
}

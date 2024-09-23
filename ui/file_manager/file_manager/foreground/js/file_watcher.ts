// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {AsyncQueue} from '../../common/js/async_util.js';
import {isFakeEntry, unwrapEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';

export type WatcherDirectoryChangedEvent =
    CustomEvent<{changedFiles: chrome.fileManagerPrivate.FileChange[]}|
                undefined>;

interface FileWatcherEventMap extends CustomEventMap {
  'watcher-directory-changed': WatcherDirectoryChangedEvent;
}

/** Watches for changes in the tracked directory. */
export class FileWatcher extends FilesEventTarget<FileWatcherEventMap> {
  private queue_ = new AsyncQueue();
  private watchedDirectoryEntry_: DirectoryEntry|null = null;
  private onDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);

  constructor() {
    super();

    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        this.onDirectoryChangedBound_);
  }

  /**
   * Stops watching (must be called before page unload).
   */
  dispose() {
    chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
        this.onDirectoryChangedBound_);
    if (this.watchedDirectoryEntry_) {
      this.resetWatchedEntry();
    }
  }

  /**
   * Called when a file in the watched directory is changed.
   * @param event Change event.
   */
  private onDirectoryChanged_(event: chrome.fileManagerPrivate.FileWatchEvent) {
    const fireWatcherDirectoryChanged =
        (changedFiles: chrome.fileManagerPrivate.FileChange[]|undefined) => {
          const eventDetails = changedFiles ? {changedFiles} : {};
          const e = new CustomEvent(
              'watcher-directory-changed', {detail: eventDetails});
          this.dispatchEvent(e);
        };

    if (this.watchedDirectoryEntry_) {
      const eventURL = event.entry.toURL();
      const watchedDirURL = this.watchedDirectoryEntry_.toURL();

      if (eventURL === watchedDirURL) {
        fireWatcherDirectoryChanged(event.changedFiles);
      } else if (watchedDirURL.startsWith(eventURL)) {
        // When watched directory is deleted by the change in parent directory,
        // notify it as watcher directory changed.
        this.watchedDirectoryEntry_.getDirectory(
            this.watchedDirectoryEntry_.fullPath, {create: false}, undefined,
            () => {
              fireWatcherDirectoryChanged(undefined);
            });
      }
    }
  }

  /**
   * Changes the watched directory. In case of a fake entry, the watch is
   * just released, since there is no reason to track a fake directory.
   *
   * @param entry Directory entry to be tracked, or the fake entry.
   */
  changeWatchedDirectory(entry: DirectoryEntry|FilesAppEntry): Promise<void> {
    if (!isFakeEntry(entry)) {
      return this.changeWatchedEntry_(unwrapEntry(entry) as DirectoryEntry);
    } else {
      return this.resetWatchedEntry();
    }
  }

  /**
   * Resets the watched entry. It's a best effort method.
   */
  resetWatchedEntry(): Promise<void> {
    // Run the tasks in the queue to avoid races.
    return new Promise<void>((fulfill) => {
      this.queue_.run(callback => {
        // Release the watched directory.
        if (this.watchedDirectoryEntry_) {
          chrome.fileManagerPrivate.removeFileWatch(
              this.watchedDirectoryEntry_, (_result: boolean|undefined) => {
                if (chrome.runtime.lastError) {
                  console.warn(`Cannot remove watcher for (redacted): ${
                      chrome.runtime.lastError.message}`);
                  console.info(`Cannot remove watcher for '${
                      this.watchedDirectoryEntry_?.toURL()}': ${
                      chrome.runtime.lastError.message}`);
                }
                // Even on error reset the watcher locally, so at least the
                // notifications are discarded.
                this.watchedDirectoryEntry_ = null;
                fulfill();
                callback();
              });
        } else {
          fulfill();
          callback();
        }
      });
    });
  }

  /**
   * Sets the watched entry to the passed directory. It's a best effort method.
   * @param entry Directory to be watched.
   */
  private changeWatchedEntry_(entry: DirectoryEntry): Promise<void> {
    return new Promise<void>((fulfill) => {
      const setEntryClosure = () => {
        // Run the tasks in the queue to avoid races.
        this.queue_.run(callback => {
          chrome.fileManagerPrivate.addFileWatch(
              entry, (_result: boolean|undefined) => {
                if (chrome.runtime.lastError) {
                  // Most probably setting the watcher is not supported on the
                  // file system type.
                  console.info(`Cannot add watcher for '${entry.toURL()}': ${
                      chrome.runtime.lastError.message}`);
                  this.watchedDirectoryEntry_ = null;
                  fulfill();
                } else {
                  assert(entry);
                  this.watchedDirectoryEntry_ = entry;
                  fulfill();
                }
                callback();
              });
        });
      };

      // Reset the watched directory first, then set the new watched directory.
      return this.resetWatchedEntry().then(setEntryClosure);
    });
  }

  /**
   * @return Current watched directory entry.
   */
  getWatchedDirectoryEntry(): DirectoryEntry|null {
    return this.watchedDirectoryEntry_;
  }
}

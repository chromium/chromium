// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getPreferences} from '../../common/js/api.js';
import type {PermutationEvent} from '../../common/js/array_data_model.js';
import {AsyncQueue, Group} from '../../common/js/async_util.js';
import {comparePath, isSameEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';
import type {FilteredVolumeManager} from '../../common/js/filtered_volume_manager.js';
import {recordSmallCount, recordUserAction} from '../../common/js/metrics.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';
import {addFolderShortcut, refreshFolderShortcut, removeFolderShortcut} from '../../state/ducks/folder_shortcuts.js';
import {getStore} from '../../state/store.js';

/**
 * The drive mount path used in the persisted storage. It must be '/drive'.
 */
const STORED_DRIVE_MOUNT_PATH = '/drive';

interface FolderShortcutsDataModelEventMap extends CustomEventMap {
  'permuted': PermutationEvent;
}

/**
 * Model for the folder shortcuts. This object is ArrayDataModel-like
 * object with additional methods for the folder shortcut feature.
 *
 * Items are always sorted by URL.
 */
export class FolderShortcutsDataModel extends
    FilesEventTarget<FolderShortcutsDataModelEventMap> {
  private array_: Array<Entry|FilesAppEntry> = [];
  private pendingPaths_ = new Set<string>();  // Hash map for easier deleting.
  private unresolvablePaths_ = new Set<string>();
  private lastDriveRootURL_: string|null = null;

  private store_ = getStore();
  private queue_ = new AsyncQueue();

  /**
   * @param volumeManager Volume manager instance.
   */
  constructor(private volumeManager_: FilteredVolumeManager) {
    super();

    // Queue to serialize resolving entries.
    this.queue_.run(
        this.volumeManager_.ensureInitialized.bind(this.volumeManager_));

    // Load the shortcuts. Runs within the queue.
    this.load_();

    // The list of folder shortcuts is persisted in the preferences.
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.reload_.bind(this));

    // If the volume info list is changed, then shortcuts have to be reloaded.
    this.volumeManager_.volumeInfoList.addEventListener(
        'permuted', this.reload_.bind(this));

    // If the drive status has changed, then shortcuts have to be re-resolved.
    this.volumeManager_.addEventListener(
        'drive-connection-changed', this.reload_.bind(this));
  }

  /**
   * @return Number of elements in the array.
   */
  get length(): number {
    return this.array_.length;
  }

  /**
   * Remembers the Drive volume's root URL used for conversions between virtual
   * paths and URLs.
   */
  private rememberLastDriveUrl_() {
    if (this.lastDriveRootURL_) {
      return;
    }
    const volumeInfo =
        this.volumeManager_.getCurrentProfileVolumeInfo(VolumeType.DRIVE);
    if (volumeInfo) {
      this.lastDriveRootURL_ = volumeInfo.fileSystem.root.toURL();
    }
  }

  /**
   * Resolves Entries from a list of stored virtual paths. Runs within a queue.
   * @param list List of virtual paths.
   */
  private processEntries_(list: string[]) {
    this.queue_.run(callback => {
      this.pendingPaths_ = new Set<string>();
      this.unresolvablePaths_ = new Set<string>();
      list.forEach(path => {
        this.pendingPaths_.add(path);
      }, this);
      callback();
    });

    this.queue_.run(queueCallback => {
      const volumeInfo =
          this.volumeManager_.getCurrentProfileVolumeInfo(VolumeType.DRIVE);
      let changed = false;
      const resolvedURLs = new Set<string>();
      this.rememberLastDriveUrl_();  // Required for conversions.

      const onResolveSuccess = (path: string, entry: Entry) => {
        if (this.pendingPaths_.has(path)) {
          this.pendingPaths_.delete(path);
        }
        if (this.unresolvablePaths_.has(path)) {
          changed = true;
          this.unresolvablePaths_.delete(path);
        }
        if (!this.exists(entry)) {
          changed = true;
          this.addInternal_(entry);
        }
        resolvedURLs.add(entry.toURL());
      };

      const onResolveFailure = (path: string, url: string|null) => {
        if (this.pendingPaths_.has(path)) {
          this.pendingPaths_.delete(path);
        }
        const existingIndex = this.getIndexByUrl_(url || '');
        if (existingIndex !== -1) {
          changed = true;
          this.removeInternal_(this.item(existingIndex)!);
        }
        // Remove the shortcut on error, only if Drive is fully online.
        // Only then we can be sure, that the error means that the directory
        // does not exist anymore.
        if (!volumeInfo ||
            this.volumeManager_.getDriveConnectionState().type !==
                chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE) {
          if (!this.unresolvablePaths_.has(path)) {
            changed = true;
            this.unresolvablePaths_.add(path);
          }
        }
        // Not adding to the model nor to the |unresolvablePaths_| means
        // that it will be removed from the persistent storage permanently after
        // the next call to save_().
      };

      // Resolve the items all at once, in parallel.
      const group = new Group();
      list.forEach(path => {
        group.add((callback) => {
          const url =
              this.lastDriveRootURL_ && this.convertStoredPathToUrl_(path);
          if (url && volumeInfo) {
            window.webkitResolveLocalFileSystemURL(
                url,
                entry => {
                  onResolveSuccess(path, entry);
                  callback();
                },
                () => {
                  onResolveFailure(path, url);
                  callback();
                });
          } else {
            onResolveFailure(path, url);
            callback();
          }
        });
      });

      // Save the model after finishing.
      group.run(() => {
        // Remove all of those old entries, which were resolved by this method.
        let index = 0;
        while (index < this.length) {
          const entry = this.item(index)!;
          if (!resolvedURLs.has(entry.toURL())) {
            this.removeInternal_(entry);
            changed = true;
          } else {
            index++;
          }
        }
        // If something changed, then save.
        if (changed) {
          this.store_.dispatch(refreshFolderShortcut({entries: this.array_}));
          this.save_();
        }
        queueCallback();
      });
    });
  }

  /**
   * Initializes the model and loads the shortcuts.
   */
  private async load_() {
    this.queue_.run(async (callback) => {
      try {
        const shortcutPaths = await this.getPersistedShortcutPaths_();
        // Record metrics.
        recordSmallCount('FolderShortcut.Count', shortcutPaths.length);

        // Resolve and add the entries to the model.
        this.processEntries_(shortcutPaths);  // Runs within a queue.
      } finally {
        callback();
      }
    });
  }

  /**
   * Fetches the shortcut paths from the persistent storage (preferences) it
   * migrates from the legacy storage.chrome.sync if needed.
   *
   */
  private async getPersistedShortcutPaths_(): Promise<string[]> {
    const prefs = await getPreferences();
    if (prefs.folderShortcuts && prefs.folderShortcuts.length) {
      return prefs.folderShortcuts;
    }

    return [];
  }

  /**
   * Reloads the model and loads the shortcuts.
   */
  private reload_() {
    this.queue_.run(async (callback) => {
      try {
        const shortcutPaths = await this.getPersistedShortcutPaths_();
        this.processEntries_(shortcutPaths);  // Runs within a queue.
      } finally {
        callback();
      }
    });
  }

  /**
   * Returns the entries in the given range as a new array instance. The
   * arguments and return value are compatible with Array.slice().
   *
   * @param begin Where to start the selection.
   * @param end Where to end the selection.
   * @return Entries in the selected range.
   */
  slice(begin: number, end?: number): Array<Entry|FilesAppEntry> {
    return this.array_.slice(begin, end);
  }

  /**
   * @param index Index of the element to be retrieved.
   * @return The value of the |index|-th element.
   */
  item(index: number): Entry|FilesAppEntry|undefined {
    return this.array_[index];
  }

  /**
   * @param value URL of the entry to be found.
   * @return Index of the element with the specified |value|.
   */
  private getIndexByUrl_(value: string): number {
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i]!.toURL() === value) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @param value Value of the element to be retrieved.
   * @return Index of the element with the specified |value|.
   */
  getIndex(value: Entry|FilesAppEntry): number {
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (isSameEntry(this.array_[i], value)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Compares 2 entries and returns a number indicating one entry comes before
   * or after or is the same as the other entry in sort order.
   *
   * @param a First entry.
   * @param b Second entry.
   * @return Returns -1, if |a| < |b|. Returns 0, if |a| === |b|.
   *     Otherwise, returns 1.
   */
  compare(a: Entry|FilesAppEntry, b: Entry|FilesAppEntry): number {
    return comparePath(a, b);
  }

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param value Value to be added into the array.
   * @return Index in the list which the element added to.
   */
  add(value: Entry|FilesAppEntry): number {
    const result = this.addInternal_(value);
    this.store_.dispatch(addFolderShortcut({entry: value}));
    recordUserAction('FolderShortcut.Add');
    this.save_();
    return result;
  }

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param value Value to be added into the array.
   * @return Index in the list which the element added to.
   */
  private addInternal_(value: Entry|FilesAppEntry): number {
    this.rememberLastDriveUrl_();  // Required for saving.

    const oldArray = this.array_.slice(0);  // Shallow copy.
    let addedIndex = -1;
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (isSameEntry(this.array_[i], value)) {
        return i;
      }

      // Since the array is sorted, new item will be added just before the first
      // larger item.
      if (this.compare(this.array_[i]!, value) >= 0) {
        this.array_.splice(i, 0, value);
        addedIndex = i;
        break;
      }
    }
    // If value is not added yet, add it at the last.
    if (addedIndex === -1) {
      this.array_.push(value);
      addedIndex = this.length;
    }

    this.firePermutedEvent_(this.calculatePermutation_(oldArray, this.array_));
    return addedIndex;
  }

  /**
   * Removes the given item from the array.
   * @param value Value to be removed from the array.
   * @return Index in the list which the element removed from.
   */
  remove(value: Entry|FilesAppEntry): number {
    const result = this.removeInternal_(value);
    if (result !== -1) {
      this.store_.dispatch(removeFolderShortcut({key: value.toURL()}));
      this.save_();
      recordUserAction('FolderShortcut.Remove');
    }
    return result;
  }

  /**
   * Removes the given item from the array.
   *
   * @param value Value to be removed from the array.
   * @return Index in the list which the element removed from.
   */
  private removeInternal_(value: Entry|FilesAppEntry): number {
    let removedIndex = -1;
    const oldArray = this.array_.slice(0);  // Shallow copy.
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (isSameEntry(this.array_[i], value)) {
        this.array_.splice(i, 1);
        removedIndex = i;
        break;
      }
    }

    if (removedIndex !== -1) {
      this.firePermutedEvent_(
          this.calculatePermutation_(oldArray, this.array_));
      return removedIndex;
    }

    // No item is removed.
    return -1;
  }

  /**
   * @param entry Entry to be checked.
   * @return True if the given |entry| exists in the array. False otherwise.
   */
  exists(entry: Entry|FilesAppEntry): boolean {
    const index = this.getIndex(entry);
    return (index >= 0);
  }

  /**
   * Saves the current array to the persistent storage (Chrome prefs).
   */
  private save_() {
    this.rememberLastDriveUrl_();
    if (!this.lastDriveRootURL_) {
      return;
    }

    // TODO(mtomasz): Migrate to URL.
    const paths = this.array_
                      .map(entry => {
                        return entry.toURL();
                      })
                      .map(this.convertUrlToStoredPath_.bind(this))
                      .filter((path): path is string => !!path)
                      .concat(...this.pendingPaths_)
                      .concat(...this.unresolvablePaths_);

    const prefs = {folderShortcuts: paths};
    chrome.fileManagerPrivate.setPreferences(prefs);
  }

  /**
   * Creates a permutation array for 'permuted' event, which is compatible with
   * a permutation array used in cr/ui/array_data_model.js.
   *
   * @param oldArray Previous array before changing.
   * @param newArray New array after changing.
   * @return Created permutation array.
   */
  private calculatePermutation_(
      oldArray: Array<Entry|FilesAppEntry>,
      newArray: Array<Entry|FilesAppEntry>): number[] {
    let oldIndex = 0;  // Index of oldArray.
    let newIndex = 0;  // Index of newArray.

    // Note that both new and old arrays are sorted.
    const permutation = [];
    for (; oldIndex < oldArray.length; oldIndex++) {
      if (newIndex >= newArray.length) {
        // oldArray[oldIndex] is deleted, which is not in the new array.
        permutation[oldIndex] = -1;
        continue;
      }

      while (newIndex < newArray.length) {
        // Unchanged item, which exists in both new and old array. But the
        // index may be changed.
        if (isSameEntry(oldArray[oldIndex], newArray[newIndex])) {
          permutation[oldIndex] = newIndex;
          newIndex++;
          break;
        }

        // oldArray[oldIndex] is deleted, which is not in the new array.
        if (this.compare(oldArray[oldIndex]!, newArray[newIndex]!) < 0) {
          permutation[oldIndex] = -1;
          break;
        }

        // In the case of this.compare(oldArray[oldIndex]) > 0:
        // newArray[newIndex] is added, which is not in the old array.
        newIndex++;
      }
    }
    return permutation;
  }

  /**
   * Fires a 'permuted' event, which is compatible with ArrayDataModel.
   * @param permutation Permutation array.
   */
  private firePermutedEvent_(permutation: number[]) {
    const permutedEvent =
        new CustomEvent(
            'permuted', {detail: {newLength: this.length, permutation}}) as
        PermutationEvent;
    this.dispatchEvent(permutedEvent);

    // Note: This model only fires 'permuted' event, because:
    // 1) 'change' event is not necessary to fire since it is covered by
    //    'permuted' event.
    // 2) 'splice' and 'sorted' events are not implemented. We have to implement
    // them when necessary.
  }

  /**
   * Called externally when one of the items is not found on the filesystem.
   * @param entry The entry which is not found.
   */
  onItemNotFoundError(entry: Entry) {
    // If Drive is online, then delete the shortcut permanently. Otherwise,
    // delete from model and add to |unresolvablePaths_|.
    if (this.volumeManager_.getDriveConnectionState().type !==
        chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE) {
      const path = this.convertUrlToStoredPath_(entry.toURL());
      // TODO(mtomasz): Add support for multi-profile.
      if (path) {
        this.unresolvablePaths_.add(path);
      }
    }
    this.removeInternal_(entry);
    this.save_();
  }

  /**
   * Converts the given "stored path" to the URL.
   *
   * This conversion is necessary because the shortcuts are not stored with
   * stored-formatted mount paths for compatibility. See http://crbug.com/336155
   * for detail.
   *
   * @param path Path in Drive with the stored drive mount path.
   * @return URL of the given path.
   */
  private convertStoredPathToUrl_(path: string): string|null {
    if (path.indexOf(STORED_DRIVE_MOUNT_PATH + '/') !== 0) {
      console.warn(path + ' is neither a drive mount path nor a stored path.');
      return null;
    }
    return this.lastDriveRootURL_ +
        encodeURIComponent(path.substr(STORED_DRIVE_MOUNT_PATH.length));
  }

  /**
   * Converts the URL to the stored-formatted path.
   *
   * See the comment of convertStoredPathToUrl_() for further information.
   *
   * @param url URL of the directory in Drive.
   * @return Path with the stored drive mount path.
   */
  private convertUrlToStoredPath_(url: string): null|string {
    // Root URLs contain a trailing slash.
    if (!this.lastDriveRootURL_ || url.indexOf(this.lastDriveRootURL_) !== 0) {
      console.warn(url + ' is not a drive URL.');
      return null;
    }

    return STORED_DRIVE_MOUNT_PATH + '/' +
        decodeURIComponent(url.substr(this.lastDriveRootURL_.length));
  }
}

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {getPreferences} from '../../common/js/api.js';
import {AsyncQueue, Group} from '../../common/js/async_util.js';
import {FilteredVolumeManager} from '../../common/js/filtered_volume_manager.js';
import {metrics} from '../../common/js/metrics.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {addFolderShortcut, refreshFolderShortcut, removeFolderShortcut} from '../../state/actions/folder_shortcuts.js';
import {getStore} from '../../state/store.js';

/**
 * The drive mount path used in the persisted storage. It must be '/drive'.
 * @type {string}
 */
const STORED_DRIVE_MOUNT_PATH = '/drive';

/**
 * Model for the folder shortcuts. This object is ArrayDataModel-like
 * object with additional methods for the folder shortcut feature.
 *
 * Items are always sorted by URL.
 */
export class FolderShortcutsDataModel extends EventTarget {
  /**
   * @param {!FilteredVolumeManager} volumeManager Volume manager instance.
   */
  constructor(volumeManager) {
    super();

    this.volumeManager_ = volumeManager;
    this.array_ = [];
    this.pendingPaths_ = {};  // Hash map for easier deleting.
    this.unresolvablePaths_ = {};
    this.lastDriveRootURL_ = null;
    this.store_ = getStore();

    // Queue to serialize resolving entries.
    this.queue_ = new AsyncQueue();
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
   * @return {number} Number of elements in the array.
   */
  get length() {
    return this.array_.length;
  }

  /**
   * Remembers the Drive volume's root URL used for conversions between virtual
   * paths and URLs.
   * @private
   */
  rememberLastDriveURL_() {
    if (this.lastDriveRootURL_) {
      return;
    }
    const volumeInfo = this.volumeManager_.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE);
    if (volumeInfo) {
      this.lastDriveRootURL_ = volumeInfo.fileSystem.root.toURL();
    }
  }

  /**
   * Resolves Entries from a list of stored virtual paths. Runs within a queue.
   * @param {Array<string>} list List of virtual paths.
   * @private
   */
  processEntries_(list) {
    this.queue_.run(callback => {
      this.pendingPaths_ = {};
      this.unresolvablePaths_ = {};
      list.forEach(function(path) {
        this.pendingPaths_[path] = true;
      }, this);
      callback();
    });

    this.queue_.run(queueCallback => {
      const volumeInfo = this.volumeManager_.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DRIVE);
      let changed = false;
      const resolvedURLs = {};
      this.rememberLastDriveURL_();  // Required for conversions.

      const onResolveSuccess = (path, entry) => {
        if (path in this.pendingPaths_) {
          delete this.pendingPaths_[path];
        }
        if (path in this.unresolvablePaths_) {
          changed = true;
          delete this.unresolvablePaths_[path];
        }
        if (!this.exists(entry)) {
          changed = true;
          this.addInternal_(entry);
        }
        resolvedURLs[entry.toURL()] = true;
      };

      const onResolveFailure = (path, url) => {
        if (path in this.pendingPaths_) {
          delete this.pendingPaths_[path];
        }
        const existingIndex = this.getIndexByURL_(url);
        if (existingIndex !== -1) {
          changed = true;
          this.removeInternal_(this.item(existingIndex));
        }
        // Remove the shortcut on error, only if Drive is fully online.
        // Only then we can be sure, that the error means that the directory
        // does not exist anymore.
        if (!volumeInfo ||
            this.volumeManager_.getDriveConnectionState().type !==
                chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE) {
          if (!this.unresolvablePaths_[path]) {
            changed = true;
            this.unresolvablePaths_[path] = true;
          }
        }
        // Not adding to the model nor to the |unresolvablePaths_| means
        // that it will be removed from the persistent storage permanently after
        // the next call to save_().
      };

      // Resolve the items all at once, in parallel.
      const group = new Group();
      list.forEach(function(path) {
        group.add(((path, callback) => {
                    const url = this.lastDriveRootURL_ &&
                        this.convertStoredPathToUrl_(path);
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
                  }).bind(null, path));
      }, this);

      // Save the model after finishing.
      group.run(() => {
        // Remove all of those old entries, which were resolved by this method.
        let index = 0;
        while (index < this.length) {
          const entry = this.item(index);
          if (!resolvedURLs[entry.toURL()]) {
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
   * @private
   */
  async load_() {
    this.queue_.run(async (callback) => {
      try {
        const shortcutPaths = await this.getPersistedShortcutPaths_();
        // Record metrics.
        metrics.recordSmallCount('FolderShortcut.Count', shortcutPaths.length);

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
   * @return {!Promise<!Array<string>>}
   * @private
   */
  async getPersistedShortcutPaths_() {
    const prefs = await getPreferences();
    if (prefs.folderShortcuts && prefs.folderShortcuts.length) {
      return prefs.folderShortcuts;
    }

    return [];
  }

  /**
   * Reloads the model and loads the shortcuts.
   * @private
   */
  reload_() {
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
   * @param {number} begin Where to start the selection.
   * @param {number=} opt_end Where to end the selection.
   * @return {Array<Entry>} Entries in the selected range.
   */
  slice(begin, opt_end) {
    return this.array_.slice(begin, opt_end);
  }

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {Entry} The value of the |index|-th element.
   */
  item(index) {
    return this.array_[index];
  }

  /**
   * @param {string} value URL of the entry to be found.
   * @return {number} Index of the element with the specified |value|.
   * @private
   */
  getIndexByURL_(value) {
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i].toURL() === value) {
        return i;
      }
    }
    return -1;
  }

  /**
   * @param {Entry} value Value of the element to be retrieved.
   * @return {number} Index of the element with the specified |value|.
   */
  getIndex(value) {
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Compares 2 entries and returns a number indicating one entry comes before
   * or after or is the same as the other entry in sort order.
   *
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {number} Returns -1, if |a| < |b|. Returns 0, if |a| === |b|.
   *     Otherwise, returns 1.
   */
  compare(a, b) {
    return util.comparePath(a, b);
  }

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   */
  add(value) {
    const result = this.addInternal_(value);
    this.store_.dispatch(addFolderShortcut({entry: value}));
    metrics.recordUserAction('FolderShortcut.Add');
    this.save_();
    return result;
  }

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   * @private
   */
  addInternal_(value) {
    this.rememberLastDriveURL_();  // Required for saving.

    const oldArray = this.array_.slice(0);  // Shallow copy.
    let addedIndex = -1;
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value)) {
        return i;
      }

      // Since the array is sorted, new item will be added just before the first
      // larger item.
      if (this.compare(this.array_[i], value) >= 0) {
        this.array_.splice(i, 0, value);
        addedIndex = i;
        break;
      }
    }
    // If value is not added yet, add it at the last.
    if (addedIndex == -1) {
      this.array_.push(value);
      addedIndex = this.length;
    }

    this.firePermutedEvent_(this.calculatePermutation_(oldArray, this.array_));
    return addedIndex;
  }

  /**
   * Removes the given item from the array.
   * @param {Entry} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   */
  remove(value) {
    const result = this.removeInternal_(value);
    if (result !== -1) {
      this.store_.dispatch(removeFolderShortcut({key: value.toURL()}));
      this.save_();
      metrics.recordUserAction('FolderShortcut.Remove');
    }
    return result;
  }

  /**
   * Removes the given item from the array.
   *
   * @param {Entry} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   * @private
   */
  removeInternal_(value) {
    let removedIndex = -1;
    const oldArray = this.array_.slice(0);  // Shallow copy.
    for (let i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value)) {
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
   * @param {Entry} entry Entry to be checked.
   * @return {boolean} True if the given |entry| exists in the array. False
   *     otherwise.
   */
  exists(entry) {
    const index = this.getIndex(entry);
    return (index >= 0);
  }

  /**
   * Saves the current array to the persistent storage (Chrome prefs).
   * @private
   */
  save_() {
    this.rememberLastDriveURL_();
    if (!this.lastDriveRootURL_) {
      return;
    }

    // TODO(mtomasz): Migrate to URL.
    const paths = this.array_
                      .map(entry => {
                        return entry.toURL();
                      })
                      .map(this.convertUrlToStoredPath_.bind(this))
                      .concat(Object.keys(this.pendingPaths_))
                      .concat(Object.keys(this.unresolvablePaths_));

    const prefs = {folderShortcuts: paths};
    chrome.fileManagerPrivate.setPreferences(prefs);
  }

  /**
   * Creates a permutation array for 'permuted' event, which is compatible with
   * a permutation array used in cr/ui/array_data_model.js.
   *
   * @param {Array<Entry>} oldArray Previous array before changing.
   * @param {Array<Entry>} newArray New array after changing.
   * @return {Array<number>} Created permutation array.
   * @private
   */
  calculatePermutation_(oldArray, newArray) {
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
        if (util.isSameEntry(oldArray[oldIndex], newArray[newIndex])) {
          permutation[oldIndex] = newIndex;
          newIndex++;
          break;
        }

        // oldArray[oldIndex] is deleted, which is not in the new array.
        if (this.compare(oldArray[oldIndex], newArray[newIndex]) < 0) {
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
   * @param {Array<number>} permutation Permutation array.
   */
  firePermutedEvent_(permutation) {
    const permutedEvent = new Event('permuted');
    permutedEvent.newLength = this.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);

    // Note: This model only fires 'permuted' event, because:
    // 1) 'change' event is not necessary to fire since it is covered by
    //    'permuted' event.
    // 2) 'splice' and 'sorted' events are not implemented. These events are
    //    not used in NavigationListModel. We have to implement them when
    //    necessary.
  }

  /**
   * Called externally when one of the items is not found on the filesystem.
   * @param {Entry} entry The entry which is not found.
   */
  onItemNotFoundError(entry) {
    // If Drive is online, then delete the shortcut permanently. Otherwise,
    // delete from model and add to |unresolvablePaths_|.
    if (this.volumeManager_.getDriveConnectionState().type !==
        chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE) {
      const path = this.convertUrlToStoredPath_(entry.toURL());
      // TODO(mtomasz): Add support for multi-profile.
      this.unresolvablePaths_[path] = true;
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
   * @param {string} path Path in Drive with the stored drive mount path.
   * @return {?string} URL of the given path.
   * @private
   */
  convertStoredPathToUrl_(path) {
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
   * @param {string} url URL of the directory in Drive.
   * @return {?string} Path with the stored drive mount path.
   * @private
   */
  convertUrlToStoredPath_(url) {
    // Root URLs contain a trailing slash.
    if (url.indexOf(this.lastDriveRootURL_) !== 0) {
      console.warn(url + ' is not a drive URL.');
      return null;
    }

    return STORED_DRIVE_MOUNT_PATH + '/' +
        decodeURIComponent(url.substr(this.lastDriveRootURL_.length));
  }
}

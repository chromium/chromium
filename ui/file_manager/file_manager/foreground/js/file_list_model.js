// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * File list.
 */
class FileListModel extends cr.ui.ArrayDataModel {
  /** @param {!MetadataModel} metadataModel */
  constructor(metadataModel) {
    super([]);

    /**
     * @private {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    // Initialize compare functions.
    this.setCompareFunction(
        'name',
        /** @type {function(*, *): number} */ (this.compareName_.bind(this)));
    this.setCompareFunction(
        'modificationTime',
        /** @type {function(*, *): number} */ (this.compareMtime_.bind(this)));
    this.setCompareFunction(
        'size',
        /** @type {function(*, *): number} */ (this.compareSize_.bind(this)));
    this.setCompareFunction(
        'type',
        /** @type {function(*, *): number} */ (this.compareType_.bind(this)));

    /**
     * Whether this file list is sorted in descending order.
     * @type {boolean}
     * @private
     */
    this.isDescendingOrder_ = false;

    /**
     * The number of folders in the list.
     * @private {number}
     */
    this.numFolders_ = 0;

    /**
     * The number of files in the list.
     * @private {number}
     */
    this.numFiles_ = 0;

    /**
     * The number of image files in the list.
     * @private {number}
     */
    this.numImageFiles_ = 0;

    /**
     * Whether to use modificationByMeTime as "Last Modified" time.
     * @private {boolean}
     */
    this.useModificationByMeTime_ = false;

    /** @private {VolumeManager} The volume manager. */
    this.volumeManager_ = null;

    /**
     * @private {EntryLocation} Used to get the label for entries when sorting
     *     by label.
     */
    this.locationInfo_ = null;
  }

  /**
   * @param {!Object} fileType Type object returned by FileType.getType().
   * @return {string} Localized string representation of file type.
   */
  static getFileTypeString(fileType) {
    // Partitions on removable volumes are treated separately, they don't
    // have translatable names.
    if (fileType.type === 'partition') {
      return fileType.subtype;
    }
    if (fileType.subtype) {
      return strf(fileType.name, fileType.subtype);
    } else {
      return str(fileType.name);
    }
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event.
   * @param {string} field Sort field.
   * @param {string} direction Sort direction.
   * @override
   */
  sort(field, direction) {
    this.isDescendingOrder_ = direction === 'desc';
    cr.ui.ArrayDataModel.prototype.sort.call(this, field, direction);
  }

  /**
   * Called before a sort happens so that you may fetch additional data
   * required for the sort.
   * @param {string} field Sort field.
   * @param {function()} callback The function to invoke when preparation
   *     is complete.
   * @override
   */
  prepareSort(field, callback) {
    // Starts the actual sorting immediately as we don't need any preparation to
    // sort the file list and we want to start actual sorting as soon as
    // possible after we get the |this.isDescendingOrder_| value in sort().
    callback();
  }

  /**
   * Removes and adds items to the model.
   *
   * The implementation is similar to cr.ui.ArrayDataModel.splice(), but this
   * has a Files app specific optimization, which sorts only the new items and
   * merge sorted lists.
   * Note that this implementation assumes following conditions.
   * - The list is always sorted.
   * - FileListModel doesn't have to do anything in prepareSort().
   *
   * @param {number} index The index of the item to update.
   * @param {number} deleteCount The number of items to remove.
   * @param {...*} var_args The items to add.
   * @return {!Array} An array with the removed items.
   * @override
   */
  splice(index, deleteCount, var_args) {
    const insertPos = Math.max(0, Math.min(index, this.indexes_.length));
    deleteCount = Math.min(deleteCount, this.indexes_.length - insertPos);

    for (let i = insertPos; i < insertPos + deleteCount; i++) {
      this.onRemoveEntryFromList_(this.array_[this.indexes_[i]]);
    }
    for (let i = 2; i < arguments.length; i++) {
      this.onAddEntryToList_(arguments[i]);
    }

    // Prepare a comparison function to sort the list.
    let comp = null;
    if (this.sortStatus.field && this.compareFunctions_) {
      const compareFunction = this.compareFunctions_[this.sortStatus.field];
      if (compareFunction) {
        const dirMultiplier = this.sortStatus.direction === 'desc' ? -1 : 1;
        comp = (a, b) => {
          return compareFunction(a, b) * dirMultiplier;
        };
      }
    }

    // Store the given new items in |newItems| and sort it before marge them to
    // the existing list.
    const newItems = [];
    for (let i = 0; i < arguments.length - 2; i++) {
      newItems.push(arguments[i + 2]);
    }
    if (comp) {
      newItems.sort(comp);
    }

    // Creating a list of existing items.
    // This doesn't include items which should be deleted by this splice() call.
    const deletedItems = [];
    const currentItems = [];
    for (let i = 0; i < this.indexes_.length; i++) {
      const item = this.array_[this.indexes_[i]];
      if (insertPos <= i && i < insertPos + deleteCount) {
        deletedItems.push(item);
      } else {
        currentItems.push(item);
      }
    }

    // Initialize splice permutation with -1s.
    // Values of undeleted items will be filled in following merge step.
    const permutation = new Array(this.indexes_.length);
    for (let i = 0; i < permutation.length; i++) {
      permutation[i] = -1;
    }

    // Merge the list of existing item and the list of new items.
    this.indexes_ = [];
    this.array_ = [];
    let p = 0;
    let q = 0;
    while (p < currentItems.length || q < newItems.length) {
      const currentIndex = p + q;
      this.indexes_.push(currentIndex);
      // Determine which should be inserted to the resulting list earlier, the
      // smallest item of unused current items or the smallest item of unused
      // new items.
      let shouldPushCurrentItem;
      if (q === newItems.length) {
        shouldPushCurrentItem = true;
      } else if (p === currentItems.length) {
        shouldPushCurrentItem = false;
      } else {
        if (comp) {
          shouldPushCurrentItem = comp(currentItems[p], newItems[q]) <= 0;
        } else {
          // If the comparater is not defined, new items should be inserted to
          // the insertion position. That is, the current items before insertion
          // position should be pushed to the resulting list earlier.
          shouldPushCurrentItem = p < insertPos;
        }
      }
      if (shouldPushCurrentItem) {
        this.array_.push(currentItems[p]);
        if (p < insertPos) {
          permutation[p] = currentIndex;
        } else {
          permutation[p + deleteCount] = currentIndex;
        }
        p++;
      } else {
        this.array_.push(newItems[q]);
        q++;
      }
    }

    // Calculate the index property of splice event.
    // If no item is inserted, it is simply the insertion/deletion position.
    // If at least one item is inserted, it should be the resulting index of the
    // item which is inserted first.
    let spliceIndex = insertPos;
    if (arguments.length > 2) {
      for (let i = 0; i < this.indexes_.length; i++) {
        if (this.array_[this.indexes_[i]] === arguments[2]) {
          spliceIndex = i;
          break;
        }
      }
    }

    // Dispatch permute/splice event.
    this.dispatchPermutedEvent_(permutation);
    // TODO(arv): Maybe unify splice and change events?
    const spliceEvent = new Event('splice');
    spliceEvent.removed = deletedItems;
    spliceEvent.added = Array.prototype.slice.call(arguments, 2);
    spliceEvent.index = spliceIndex;
    this.dispatchEvent(spliceEvent);

    return deletedItems;
  }

  /**
   * @override
   */
  replaceItem(oldItem, newItem) {
    this.onRemoveEntryFromList_(oldItem);
    this.onAddEntryToList_(newItem);

    cr.ui.ArrayDataModel.prototype.replaceItem.apply(this, arguments);
  }

  /**
   * Returns the number of files in this file list.
   * @return {number} The number of files.
   */
  getFileCount() {
    return this.numFiles_;
  }

  /**
   * Returns the number of folders in this file list.
   * @return {number} The number of folders.
   */
  getFolderCount() {
    return this.numFolders_;
  }

  /**
   * Returns true if image files are dominant in this file list (i.e. 80% or
   * more files are images).
   * @return {boolean}
   */
  isImageDominant() {
    return this.numFiles_ > 0 && this.numImageFiles_ * 10 >= this.numFiles_ * 8;
  }

  /**
   * Sets whether to use modificationByMeTime as "Last Modified" time.
   * @param {boolean} useModificationByMeTime
   */
  setUseModificationByMeTime(useModificationByMeTime) {
    this.useModificationByMeTime_ = useModificationByMeTime;
  }

  /**
   * Updates the statistics about contents when new entry is about to be added.
   * @param {Entry} entry Entry of the new item.
   * @private
   */
  onAddEntryToList_(entry) {
    if (entry.isDirectory) {
      this.numFolders_++;
    } else {
      this.numFiles_++;
    }

    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            .contentMimeType;
    if (FileType.isImage(entry, mimeType) || FileType.isRaw(entry, mimeType)) {
      this.numImageFiles_++;
    }
  }

  /**
   * Updates the statistics about contents when an entry is about to be removed.
   * @param {Entry} entry Entry of the item to be removed.
   * @private
   */
  onRemoveEntryFromList_(entry) {
    if (entry.isDirectory) {
      this.numFolders_--;
    } else {
      this.numFiles_--;
    }

    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            .contentMimeType;
    if (FileType.isImage(entry, mimeType) || FileType.isRaw(entry, mimeType)) {
      this.numImageFiles_--;
    }
  }

  /**
   * Compares entries by name.
   * @param {!Entry} a First entry.
   * @param {!Entry} b Second entry.
   * @return {number} Compare result.
   * @private
   */
  compareName_(a, b) {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    return util.compareName(a, b);
  }

  /**
   * Compares entries by label (i18n name).
   * @param {!Entry} a First entry.
   * @param {!Entry} b Second entry.
   * @return {number} Compare result.
   * @private
   */
  compareLabel_(a, b) {
    // Set locationInfo once because we only compare within the same volume.
    if (!this.locationInfo_ && this.volumeManager_) {
      this.locationInfo_ = this.volumeManager_.getLocationInfo(a);
    }

    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    return util.compareLabel(this.locationInfo_, a, b);
  }

  /**
   * Compares entries by mtime first, then by name.
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {number} Compare result.
   * @private
   */
  compareMtime_(a, b) {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties = this.metadataModel_.getCache(
        [a, b], ['modificationTime', 'modificationByMeTime']);
    const aTime = this.getMtime_(properties[0]);
    const bTime = this.getMtime_(properties[1]);

    if (aTime > bTime) {
      return 1;
    }

    if (aTime < bTime) {
      return -1;
    }

    return util.compareName(a, b);
  }

  /**
   * Returns the modification time from a properties object.
   * "Modification time" can be modificationTime or modificationByMeTime
   * depending on this.useModificationByMeTime_.
   * @param {!Object} properties Properties object.
   * @return {number} Modification time.
   * @private
   */
  getMtime_(properties) {
    if (this.useModificationByMeTime_) {
      return properties.modificationByMeTime || properties.modificationTime ||
          0;
    }
    return properties.modificationTime || 0;
  }

  /**
   * Compares entries by size first, then by name.
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {number} Compare result.
   * @private
   */
  compareSize_(a, b) {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties = this.metadataModel_.getCache([a, b], ['size']);
    const aSize = properties[0].size || 0;
    const bSize = properties[1].size || 0;

    return aSize !== bSize ? aSize - bSize : util.compareName(a, b);
  }

  /**
   * Compares entries by type first, then by subtype and then by name.
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {number} Compare result.
   * @private
   */
  compareType_(a, b) {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties =
        this.metadataModel_.getCache([a, b], ['contentMimeType']);
    const aType = FileListModel.getFileTypeString(
        FileType.getType(a, properties[0].contentMimeType));
    const bType = FileListModel.getFileTypeString(
        FileType.getType(b, properties[1].contentMimeType));

    const result = util.collator.compare(aType, bType);
    return result !== 0 ? result : util.compareName(a, b);
  }

  /**
   * @param {!VolumeManager} volumeManager The volume manager.
   */
  InitNewDirContents(volumeManager) {
    this.volumeManager_ = volumeManager;
    // Clear the location info, it's reset by compareLabel_ when needed.
    this.locationInfo_ = null;
    // Initialize compare function based on Labels.
    this.setCompareFunction(
        'name',
        /** @type {function(*, *): number} */ (this.compareLabel_.bind(this)));
  }
}

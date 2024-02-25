// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {EntryLocation} from '../../background/js/entry_location_impl.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {compareLabel, compareName} from '../../common/js/entry_utils.js';
import type {FileExtensionType} from '../../common/js/file_type.js';
import {getType, isImage, isRaw} from '../../common/js/file_type.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {getRecentDateBucket, getTranslationKeyForDateBucket} from '../../common/js/recent_date_bucket.js';
import {collator, str, strf} from '../../common/js/translations.js';

import type {MetadataItem} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';

export const GROUP_BY_FIELD_MODIFICATION_TIME = 'modificationTime';
export const GROUP_BY_FIELD_DIRECTORY = 'isDirectory';

const FIELDS_SUPPORT_GROUP_BY = new Set([
  GROUP_BY_FIELD_MODIFICATION_TIME,
  GROUP_BY_FIELD_DIRECTORY,
]);

type UniversalEntry = FilesAppEntry|Entry;

/**
 * Currently we only support group by modificationTime or isDirectory, so the
 * group value can only be one of them.
 */
export type GroupValue = chrome.fileManagerPrivate.RecentDateBucket|boolean;

/**
 * This represents a group header.
 */
export interface GroupHeader {
  /** The start index of this group. */
  startIndex: number;
  /** The end index of this group. */
  endIndex: number;
  /** The actual group value. */
  group: (GroupValue|undefined);
  /** The group label. */
  label: string;
}

/**
 * This represents the snapshot of a groupBy result.
 */
interface GroupBySnapshot {
  /** When groupBy is calculated, what the sort order is. */
  sortDirection: string;
  /** The groupBy results, each item is a `GroupHeader` type. */
  groups: GroupHeader[];
}

/**
 * File list.
 */
export class FileListModel extends ArrayDataModel<UniversalEntry> {
  /**
   * Whether this file list is sorted in descending order.
   */
  private isDescendingOrder_: boolean = false;

  /**
   * The number of folders in the list.
   */
  private numFolders_: number = 0;

  /**
   * The number of files in the list.
   */
  private numFiles_: number = 0;

  /**
   * The number of image files in the list.
   */
  private numImageFiles_: number = 0;

  /**
   * Whether to use modificationByMeTime as "Last Modified" time.
   */
  private useModificationByMeTime_: boolean = false;

  /**
   * The volume manager.
   */
  private volumeManager_: null|VolumeManager = null;

  /**
   * Used to get the label for entries when
   * sorting by label.
   */
  private locationInfo_: null|EntryLocation = null;

  hasGroupHeadingBeforeSort: boolean = false;

  /**
   * The field to do group by on.
   */
  private groupByField_: string|null = null;

  /**
   * The key is the field name which is used by groupBy. The value is a
   * object with type GroupBySnapshot.
   *
   */
  private groupBySnapshot_: Record<string, GroupBySnapshot> =
      Array.from(FIELDS_SUPPORT_GROUP_BY)
          .reduce((acc: Record<string, GroupBySnapshot>, field) => {
            acc[field] = {
              sortDirection: 'asc',
              groups: [],
            };
            return acc;
          }, {});

  constructor(private readonly metadataModel_: MetadataModel) {
    super([]);

    // Initialize compare functions.
    this.setCompareFunction('name', this.compareName_.bind(this));
    this.setCompareFunction('modificationTime', this.compareMtime_.bind(this));
    this.setCompareFunction('size', this.compareSize_.bind(this));
    this.setCompareFunction('type', this.compareType_.bind(this));
  }

  /**
   * @param fileType Type object returned by getType().
   * @return Localized string representation of file type.
   */
  static getFileTypeString(fileType: FileExtensionType): string {
    // Partitions on removable volumes are treated separately, they don't
    // have translatable names.
    if (fileType.type === 'partition') {
      return fileType.subtype;
    }
    if (fileType.subtype) {
      return strf(fileType.translationKey, fileType.subtype);
    } else {
      return str(fileType.translationKey);
    }
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event.
   * @param field Sort field.
   * @param direction Sort direction.
   */
  override sort(field: string, direction: string) {
    this.hasGroupHeadingBeforeSort = this.shouldShowGroupHeading();
    this.isDescendingOrder_ = direction === 'desc';
    ArrayDataModel.prototype.sort.call(this, field, direction);
  }

  /**
   * Removes and adds items to the model.
   *
   * The implementation is similar to ArrayDataModel.splice(), but this
   * has a Files app specific optimization, which sorts only the new items and
   * merge sorted lists.
   * Note that this implementation assumes that the list is always sorted.
   *
   * @param index The index of the item to update.
   * @param deleteCount The number of items to remove.
   * @param args The items to add.
   * @return An array with the removed items.
   */
  override splice(
      index: number, deleteCount: number,
      ...args: UniversalEntry[]): UniversalEntry[] {
    const insertPos = Math.max(0, Math.min(index, this.indexes_.length));
    deleteCount = Math.min(deleteCount, this.indexes_.length - insertPos);

    for (let i = insertPos; i < insertPos + deleteCount; i++) {
      this.onRemoveEntryFromList_(this.array_[this.indexes_[i]!]!);
    }
    for (const arg of args) {
      this.onAddEntryToList_(arg);
    }

    // Prepare a comparison function to sort the list.
    let comp = null;
    if (this.sortStatus.field && this.compareFunctions_) {
      const compareFunction = this.compareFunctions_[this.sortStatus.field];
      if (compareFunction) {
        const dirMultiplier = this.sortStatus.direction === 'desc' ? -1 : 1;
        comp = (a: UniversalEntry, b: UniversalEntry) => {
          return compareFunction(a, b) * dirMultiplier;
        };
      }
    }

    // Store the given new items in |newItems| and sort it before marge them to
    // the existing list.
    const newItems = [];
    for (const arg of args) {
      newItems.push(arg);
    }
    if (comp) {
      newItems.sort(comp);
    }

    // Creating a list of existing items.
    // This doesn't include items which should be deleted by this splice() call.
    const deletedItems: UniversalEntry[] = [];
    const currentItems: UniversalEntry[] = [];
    for (let i = 0; i < this.indexes_.length; i++) {
      const item = this.array_[this.indexes_[i]!]!;
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
          shouldPushCurrentItem = comp(currentItems[p]!, newItems[q]!) <= 0;
        } else {
          // If the comparator is not defined, new items should be inserted to
          // the insertion position. That is, the current items before insertion
          // position should be pushed to the resulting list earlier.
          shouldPushCurrentItem = p < insertPos;
        }
      }
      if (shouldPushCurrentItem) {
        this.array_.push(currentItems[p]!);
        if (p < insertPos) {
          permutation[p] = currentIndex;
        } else {
          permutation[p + deleteCount] = currentIndex;
        }
        p++;
      } else {
        this.array_.push(newItems[q]!);
        q++;
      }
    }

    // Calculate the index property of splice event.
    // If no item is inserted, it is simply the insertion/deletion position.
    // If at least one item is inserted, it should be the resulting index of the
    // item which is inserted first.
    let spliceIndex = insertPos;
    if (args.length > 0) {
      for (let i = 0; i < this.indexes_.length; i++) {
        if (this.array_[this.indexes_[i]!] === args[0]) {
          spliceIndex = i;
          break;
        }
      }
    }

    // Dispatch permute/splice event.
    this.dispatchPermutedEvent_(permutation);
    // TODO(arv): Maybe unify splice and change events?
    const spliceEvent = new CustomEvent('splice', {
      detail: {
        removed: deletedItems,
        added: args,
        index: spliceIndex,
      },
    });
    this.dispatchEvent(spliceEvent);

    this.updateGroupBySnapshot_();

    return deletedItems;
  }

  /**
   */
  override replaceItem(oldItem: UniversalEntry, newItem: UniversalEntry) {
    this.onRemoveEntryFromList_(oldItem);
    this.onAddEntryToList_(newItem);

    super.replaceItem(oldItem, newItem);
  }

  /**
   * Returns the number of files in this file list.
   * @return The number of files.
   */
  getFileCount(): number {
    return this.numFiles_;
  }

  /**
   * Returns the number of folders in this file list.
   * @return The number of folders.
   */
  getFolderCount(): number {
    return this.numFolders_;
  }

  /**
   * Sets whether to use modificationByMeTime as "Last Modified" time.
   */
  setUseModificationByMeTime(useModificationByMeTime: boolean) {
    this.useModificationByMeTime_ = useModificationByMeTime;
  }

  /**
   * Updates the statistics about contents when new entry is about to be added.
   * @param entry Entry of the new item.
   */
  private onAddEntryToList_(entry: UniversalEntry) {
    if (entry.isDirectory) {
      this.numFolders_++;
    } else {
      this.numFiles_++;
    }

    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            ?.contentMimeType;
    if (isImage(entry, mimeType) || isRaw(entry, mimeType)) {
      this.numImageFiles_++;
    }
  }

  /**
   * Updates the statistics about contents when an entry is about to be removed.
   * @param entry Entry of the item to be removed.
   */
  private onRemoveEntryFromList_(entry: UniversalEntry) {
    if (entry.isDirectory) {
      this.numFolders_--;
    } else {
      this.numFiles_--;
    }

    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            ?.contentMimeType;
    if (isImage(entry, mimeType) || isRaw(entry, mimeType)) {
      this.numImageFiles_--;
    }
  }

  /**
   * Compares entries by name.
   * @param a First entry.
   * @param b Second entry.
   * @return Compare result.
   */
  private compareName_(a: UniversalEntry, b: UniversalEntry): number {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    return compareName(a, b);
  }

  /**
   * Compares entries by label (i18n name).
   * @param a First entry.
   * @param b Second entry.
   * @return Compare result.
   */
  private compareLabel_(a: UniversalEntry, b: UniversalEntry): number {
    // Set locationInfo once because we only compare within the same volume.
    if (!this.locationInfo_ && this.volumeManager_) {
      this.locationInfo_ = this.volumeManager_.getLocationInfo(a);
    }

    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    return compareLabel(this.locationInfo_!, a, b);
  }

  /**
   * Compares entries by mtime first, then by name.
   * @param a First entry.
   * @param b Second entry.
   * @return Compare result.
   */
  private compareMtime_(a: UniversalEntry, b: UniversalEntry): number {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties = this.metadataModel_.getCache(
        [a, b], ['modificationTime', 'modificationByMeTime']);
    const aTime = this.getMtime_(properties[0]!);
    const bTime = this.getMtime_(properties[1]!);

    if (aTime > bTime) {
      return 1;
    }

    if (aTime < bTime) {
      return -1;
    }

    return compareName(a, b);
  }

  /**
   * Returns the modification time from a properties object.
   * "Modification time" can be modificationTime or modificationByMeTime
   * depending on this.useModificationByMeTime_.
   * @param properties Properties object.
   * @return Modification time.
   */
  private getMtime_(properties: MetadataItem): number|Date {
    if (this.useModificationByMeTime_) {
      return properties.modificationByMeTime || properties.modificationTime ||
          0;
    }
    return properties.modificationTime || 0;
  }

  /**
   * Compares entries by size first, then by name.
   * @param a First entry.
   * @param b Second entry.
   * @return Compare result.
   */
  private compareSize_(a: UniversalEntry, b: UniversalEntry): number {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties = this.metadataModel_.getCache([a, b], ['size']);
    const aSize = properties[0]!.size || 0;
    const bSize = properties[1]!.size || 0;

    return aSize !== bSize ? aSize - bSize : compareName(a, b);
  }

  /**
   * Compares entries by type first, then by subtype and then by name.
   * @param a First entry.
   * @param b Second entry.
   * @return Compare result.
   */
  private compareType_(a: UniversalEntry, b: UniversalEntry): number {
    // Directories always precede files.
    if (a.isDirectory !== b.isDirectory) {
      return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;
    }

    const properties =
        this.metadataModel_.getCache([a, b], ['contentMimeType']);
    const aType = FileListModel.getFileTypeString(
        getType(a, properties[0]!.contentMimeType));
    const bType = FileListModel.getFileTypeString(
        getType(b, properties[1]!.contentMimeType));

    const result = collator.compare(aType, bType);
    return result !== 0 ? result : compareName(a, b);
  }

  initNewDirContents(volumeManager: VolumeManager) {
    this.volumeManager_ = volumeManager;
    // Clear the location info, it's reset by compareLabel_ when needed.
    this.locationInfo_ = null;
    // Initialize compare function based on Labels.
    this.setCompareFunction('name', this.compareLabel_.bind(this));
  }

  get groupByField(): string|null {
    return this.groupByField_;
  }

  /**
   * @param field the field to group by.
   */
  set groupByField(field: string|null) {
    this.groupByField_ = field;
    if (!field || this.groupBySnapshot_[field]?.groups.length === 0) {
      this.updateGroupBySnapshot_();
    }
  }

  /**
   * Should the current list model show group heading or not.
   */
  shouldShowGroupHeading(): boolean {
    if (!this.groupByField_) {
      return false;
    }
    // GroupBy modification time is only valid when the current sort field is
    // modification time.
    if (this.groupByField_ === GROUP_BY_FIELD_MODIFICATION_TIME) {
      return this.sortStatus.field === this.groupByField_;
    }
    return FIELDS_SUPPORT_GROUP_BY.has(this.groupByField_);
  }

  /**
   * @param item Item in the file list model.
   * @param now Timestamp represents now.
   */
  private getGroupForModificationTime_(item: UniversalEntry, now: number):
      chrome.fileManagerPrivate.RecentDateBucket {
    const properties = this.metadataModel_.getCache(
        [item], ['modificationTime', 'modificationByMeTime']);
    return getRecentDateBucket(
        new Date(this.getMtime_(properties[0]!)), new Date(now));
  }

  /**
   * @param item Item in the file list model.
   */
  private getGroupForDirectory_(item: UniversalEntry): boolean {
    return item.isDirectory;
  }

  private getGroupLabel_(value: GroupValue|undefined): string {
    switch (this.groupByField_) {
      case GROUP_BY_FIELD_MODIFICATION_TIME:
        const dateBucket = value as chrome.fileManagerPrivate.RecentDateBucket;
        return str(getTranslationKeyForDateBucket(dateBucket));
      case GROUP_BY_FIELD_DIRECTORY:
        const isDirectory = value as boolean;
        return isDirectory ? str('GRID_VIEW_FOLDERS_TITLE') :
                             str('GRID_VIEW_FILES_TITLE');
      default:
        return '';
    }
  }

  /**
   * Update the GroupBy snapshot by the existing sort field.
   */
  private updateGroupBySnapshot_() {
    if (!this.shouldShowGroupHeading()) {
      return;
    }
    assert(this.groupByField_);
    const snapshot: GroupBySnapshot =
        this.groupBySnapshot_[this.groupByField_!]!;
    assert(snapshot);
    snapshot.sortDirection = this.sortStatus.direction!;
    snapshot.groups = [];

    const now = Date.now();
    let prevItemGroup = null;
    for (let i = 0; i < this.length; i++) {
      const item = this.item(i)!;
      let curItemGroup;
      if (this.groupByField_ === GROUP_BY_FIELD_MODIFICATION_TIME) {
        curItemGroup = this.getGroupForModificationTime_(item, now);
      } else if (this.groupByField_ === GROUP_BY_FIELD_DIRECTORY) {
        curItemGroup = this.getGroupForDirectory_(item);
      }
      if (prevItemGroup !== curItemGroup) {
        if (i > 0) {
          snapshot.groups[snapshot.groups.length - 1]!.endIndex = i - 1;
        }
        snapshot.groups.push({
          startIndex: i,
          endIndex: -1,
          group: curItemGroup,
          label: this.getGroupLabel_(curItemGroup),
        });
      }
      prevItemGroup = curItemGroup;
    }
    if (snapshot.groups.length > 0) {
      // The last element is always the end of the last group.
      snapshot.groups[snapshot.groups.length - 1]!.endIndex = this.length - 1;
    }
  }

  /**
   * Refresh the group by data, e.g. when date modified changes due to
   * timezone change.
   */
  refreshGroupBySnapshot() {
    if (this.groupByField_ === GROUP_BY_FIELD_MODIFICATION_TIME) {
      this.updateGroupBySnapshot_();
    }
  }

  /**
   * Return the groupBy snapshot.
   */
  getGroupBySnapshot(): GroupHeader[] {
    if (!this.shouldShowGroupHeading()) {
      return [];
    }
    assert(this.groupByField_);
    const snapshot = this.groupBySnapshot_[this.groupByField_!]!;
    if (this.groupByField_ === GROUP_BY_FIELD_MODIFICATION_TIME) {
      if (this.sortStatus.direction === snapshot.sortDirection) {
        return snapshot.groups;
      }
      // Why are we calculating reverse order data in the snapshot instead
      // of calculating it inside sort() function? It's because redraw can
      // happen before sort() finishes, if we generate reverse order data
      // at the end of sort(), that might be too late for redraw.
      const reversedGroups = Array.from(snapshot.groups);
      reversedGroups.reverse();
      return reversedGroups.map(group => {
        return {
          startIndex: this.length - 1 - group.endIndex,
          endIndex: this.length - 1 - group.startIndex,
          group: group.group,
          label: group.label,
        };
      });
    }
    // Grid view Folders/Files group order never changes, e.g. Folders group
    // always shows first, and then Files group.
    if (this.groupByField_ === GROUP_BY_FIELD_DIRECTORY) {
      return snapshot.groups;
    }
    return [];
  }
}

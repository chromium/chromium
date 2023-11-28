// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {isOneDrive} from '../../common/js/entry_utils.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isArcVmEnabled, isGuestOsEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {str} from '../../common/js/translations.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {DialogType} from '../../externs/ts/state.js';
import {getStore} from '../../state/store.js';

import {AndroidAppListModel} from './android_app_list_model.js';
import {DirectoryModel} from './directory_model.js';
import {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';

/**
 * @enum {string}
 */
export const NavigationModelItemType = {
  SHORTCUT: 'shortcut',
  VOLUME: 'volume',
  RECENT: 'recent',
  CROSTINI: 'crostini',
  GUEST_OS: 'guest-os',
  ENTRY_LIST: 'entry-list',
  DRIVE: 'drive',
  ANDROID_APP: 'android-app',
  TRASH: 'trash',
};

/**
 * Navigation List sections. A section is just a visual grouping of some items.
 *
 * Sections:
 *      - TOP: Recents, Shortcuts.
 *      - MY_FILES: My Files (which includes Downloads, Crostini and Arc++ as
 *                  its children).
 *      - REMOVABLE: Archives, MTPs, Media Views and Removables.
 *      - GOOGLE_DRIVE: Just Google Drive.
 *      - ODFS: Just ODFS.
 *      - CLOUD: All other cloud: SMBs, FSPs and Documents Providers.
 *      - ANDROID_APPS: ANDROID picker apps.
 * @enum {string}
 */
export const NavigationSection = {
  TOP: 'top',
  MY_FILES: 'my_files',
  REMOVABLE: 'removable',
  GOOGLE_DRIVE: 'google_drive',
  ODFS: 'odfs',
  CLOUD: 'cloud',
  ANDROID_APPS: 'android_apps',
};

/**
 * Base item of NavigationListModel. Should not be created directly.
 */
export class NavigationModelItem {
  /**
   * @param {string} label
   * @param {NavigationModelItemType} type
   */
  constructor(label, type) {
    this.label_ = label;
    this.type_ = type;

    /**
     * @type {boolean} whether this item is disabled for selection.
     */
    this.disabled_ = false;

    /**
     * @type {NavigationSection} section which this item belongs to.
     */
    this.section_ = NavigationSection.TOP;

    /** @type {number} original order when returned from VolumeManager. */
    this.originalOrder_ = -1;
  }

  get label() {
    return this.label_;
  }

  get type() {
    return this.type_;
  }

  /** @return {boolean} */
  get disabled() {
    return this.disabled_;
  }

  /** @param {boolean} disabled */
  set disabled(disabled) {
    this.disabled_ = disabled;
  }

  /** @return {NavigationSection} */
  get section() {
    return this.section_;
  }

  /** @param {NavigationSection} section */
  set section(section) {
    this.section_ = section;
  }

  /** @return {number} */
  get originalOrder() {
    return this.originalOrder_;
  }

  set originalOrder(order) {
    this.originalOrder_ = order;
  }
}

/**
 * Item of NavigationListModel for shortcuts.
 */
export class NavigationModelShortcutItem extends NavigationModelItem {
  /**
   * @param {string} label Label.
   * @param {!DirectoryEntry} entry Entry. Cannot be null.
   */
  constructor(label, entry) {
    super(label, NavigationModelItemType.SHORTCUT);
    this.entry_ = entry;
  }

  get entry() {
    return this.entry_;
  }
}

/**
 * Item of NavigationListModel for Android apps.
 */
export class NavigationModelAndroidAppItem extends NavigationModelItem {
  /**
   * @param {!chrome.fileManagerPrivate.AndroidApp} androidApp Android app.
   *     Cannot be null.
   */
  constructor(androidApp) {
    super(androidApp.name, NavigationModelItemType.ANDROID_APP);

    /** @private @type {!chrome.fileManagerPrivate.AndroidApp} */
    this.androidApp_ = androidApp;
  }

  /** @return {!chrome.fileManagerPrivate.AndroidApp} */
  get androidApp() {
    return this.androidApp_;
  }
}

/**
 * Item of NavigationListModel for volumes.
 */
export class NavigationModelVolumeItem extends NavigationModelItem {
  /**
   * @param {string} label Label.
   * @param {!import('../../externs/volume_info.js').VolumeInfo} volumeInfo
   *     Volume info for the volume. Cannot be null.
   */
  constructor(label, volumeInfo) {
    super(label, NavigationModelItemType.VOLUME);
    this.volumeInfo_ = volumeInfo;
    // Start resolving the display root because it is used
    // for determining executability of commands.
    this.volumeInfo_.resolveDisplayRoot(() => {}, () => {});
  }

  get volumeInfo() {
    return this.volumeInfo_;
  }
}

/**
 * Item of NavigationListModel for a fake item such as Recent or Linux files.
 */
export class NavigationModelFakeItem extends NavigationModelItem {
  /**
   * @param {string} label Label on the menu button.
   * @param {NavigationModelItemType} type
   * @param {!FilesAppEntry} entry Fake entry for the root folder.
   */
  constructor(label, type, entry) {
    super(label, type);
    this.entry_ = entry;
  }

  get entry() {
    return this.entry_;
  }
}

/**
 * A navigation list model. This model combines multiple models.
 */
export class NavigationListModel extends EventTarget {
  /**
   * @param {!import('../../externs/volume_manager.js').VolumeManager}
   *     volumeManager VolumeManager instance.
   * @param {!FolderShortcutsDataModel} shortcutListModel The list of folder
   *     shortcut.
   * @param {?NavigationModelFakeItem} recentModelItem Recent folder.
   * @param {!DirectoryModel} directoryModel
   * @param {!AndroidAppListModel} androidAppListModel
   * @param {!DialogType} dialogType
   */
  constructor(
      volumeManager, shortcutListModel, recentModelItem, directoryModel,
      androidAppListModel, dialogType) {
    super();

    /**
     * @private @type {!import('../../externs/volume_manager.js').VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private @type {!FolderShortcutsDataModel}
     * @const
     */
    this.shortcutListModel_ = shortcutListModel;

    /**
     * @private @type {NavigationModelFakeItem}
     * @const
     */
    // @ts-ignore: error TS2322: Type 'NavigationModelFakeItem | null' is not
    // assignable to type 'NavigationModelFakeItem'.
    this.recentModelItem_ = recentModelItem;

    /**
     * @private @type {!DirectoryModel}
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private @type {!AndroidAppListModel}
     */
    this.androidAppListModel_ = androidAppListModel;

    /**
     * @private @type {!DialogType}
     */
    this.dialogType_ = dialogType;

    /**
     * Root folder for crostini Linux files.
     * This field will be modified when crostini is enabled/disabled.
     * @private @type {NavigationModelFakeItem}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'NavigationModelFakeItem'.
    this.linuxFilesItem_ = null;

    /**
     * Root folders for Guest OS files.
     * This field will be modified when new guests are added/removed.
     * @private @type {!Array<!NavigationModelFakeItem>}
     */
    // @ts-ignore: error TS7008: Member 'guestOsPlaceholders_' implicitly has an
    // 'any[]' type.
    this.guestOsPlaceholders_ = [];

    /**
     * Root folder for trash.
     * @private @type {?NavigationModelFakeItem}
     */
    this.trashItem_ = null;

    /**
     * NavigationModel for MyFiles, since DirectoryTree expect it to be always
     * the same reference we keep the initial reference for reuse.
     * @private @type {NavigationModelFakeItem}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'NavigationModelFakeItem'.
    this.myFilesModel_ = null;

    /**
     * A collection of NavigationModel objects for Removable partition groups.
     * Store the reference to each model here since DirectoryTree expects it to
     * always have the same reference.
     * @private @type {!Map<string, !NavigationModelFakeItem>}
     */
    this.removableModels_ = new Map();

    /**
     * All root navigation items in display order.
     * @private @type {!Array<!NavigationModelItem>}
     */
    // @ts-ignore: error TS7008: Member 'navigationItems_' implicitly has an
    // 'any[]' type.
    this.navigationItems_ = [];

    /** @private @type {?NavigationModelFakeItem} */
    this.fakeDriveItem_;

    // @ts-ignore: error TS7006: Parameter 'volumeInfo' implicitly has an 'any'
    // type.
    const volumeInfoToModelItem = volumeInfo => {
      return new NavigationModelVolumeItem(volumeInfo.label, volumeInfo);
    };

    // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any' type.
    const entryToModelItem = entry => {
      const item = new NavigationModelShortcutItem(entry.name, entry);
      return item;
    };

    /**
     * Type of updated list.
     * @enum {number}
     * @const
     */
    const ListType = {VOLUME_LIST: 1, SHORTCUT_LIST: 2, ANDROID_APP_LIST: 3};
    Object.freeze(ListType);

    // Generates this.volumeList_ and this.shortcutList_ from the models.
    this.volumeList_ = [];
    for (let i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
      this.volumeList_.push(
          volumeInfoToModelItem(this.volumeManager_.volumeInfoList.item(i)));
    }

    this.shortcutList_ = [];
    for (let i = 0; i < this.shortcutListModel_.length; i++) {
      const shortcutEntry =
          /** @type {!Entry} */ (this.shortcutListModel_.item(i));
      // @ts-ignore: error TS6133: 'volumeInfo' is declared but its value is
      // never read.
      const volumeInfo = this.volumeManager_.getVolumeInfo(shortcutEntry);
      this.shortcutList_.push(entryToModelItem(shortcutEntry));
    }

    this.androidAppList_ = [];
    for (let i = 0; i < this.androidAppListModel_.length(); i++) {
      this.androidAppList_.push(this.androidAppListModel_.item(i));
    }

    // Reorder volumes, shortcuts, and optional items for initial display.
    this.refreshNavigationItems();

    // Generates a combined 'permuted' event from an event of either volumeList
    // or shortcutList.
    // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
    const permutedHandler = function(listType, event) {
      let permutation;

      // Build the volumeList.
      if (listType == ListType.VOLUME_LIST) {
        // The volume is mounted or unmounted.
        const newList = [];

        // Use the old instances if they just move.
        for (let i = 0; i < event.permutation.length; i++) {
          if (event.permutation[i] >= 0) {
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            newList[event.permutation[i]] = this.volumeList_[i];
          }
        }

        // Create missing instances.
        for (let i = 0; i < event.newLength; i++) {
          if (!newList[i]) {
            newList[i] = volumeInfoToModelItem(
                // @ts-ignore: error TS2339: Property 'volumeManager_' does not
                // exist on type 'permutedHandler'.
                this.volumeManager_.volumeInfoList.item(i));
          }
        }
        this.volumeList_ = newList;

        permutation = event.permutation.slice();

        // shortcutList part has not been changed, so the permutation should be
        // just identity mapping with a shift.
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        for (let i = 0; i < this.shortcutList_.length; i++) {
          permutation.push(i + this.volumeList_.length);
        }
      } else if (listType == ListType.SHORTCUT_LIST) {
        // Build the shortcutList.

        // volumeList part has not been changed, so the permutation should be
        // identity mapping.

        permutation = [];
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        for (let i = 0; i < this.volumeList_.length; i++) {
          permutation[i] = i;
        }

        let modelIndex = 0;
        let oldListIndex = 0;
        const newList = [];
        // @ts-ignore: error TS2551: Property 'shortcutListModel_' does not
        // exist on type 'permutedHandler'. Did you mean 'shortcutList_'?
        while (modelIndex < this.shortcutListModel_.length &&
               // @ts-ignore: error TS2532: Object is possibly 'undefined'.
               oldListIndex < this.shortcutList_.length) {
          // @ts-ignore: error TS2551: Property 'shortcutListModel_' does not
          // exist on type 'permutedHandler'. Did you mean 'shortcutList_'?
          const shortcutEntry = this.shortcutListModel_.item(modelIndex);
          // @ts-ignore: error TS2551: Property 'shortcutListModel_' does not
          // exist on type 'permutedHandler'. Did you mean 'shortcutList_'?
          const cmp = this.shortcutListModel_.compare(
              /** @type {Entry} */ (shortcutEntry),
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              this.shortcutList_[oldListIndex].entry);
          if (cmp > 0) {
            // The shortcut at shortcutList_[oldListIndex] is removed.
            permutation.push(-1);
            oldListIndex++;
            continue;
          }

          if (cmp === 0) {
            // Reuse the old instance.
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            permutation.push(newList.length + this.volumeList_.length);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            newList.push(this.shortcutList_[oldListIndex]);
            oldListIndex++;
          } else {
            // We needs to create a new instance for the shortcut entry.
            newList.push(entryToModelItem(shortcutEntry));
          }
          modelIndex++;
        }

        // Add remaining (new) shortcuts if necessary.
        // @ts-ignore: error TS2551: Property 'shortcutListModel_' does not
        // exist on type 'permutedHandler'. Did you mean 'shortcutList_'?
        for (; modelIndex < this.shortcutListModel_.length; modelIndex++) {
          // @ts-ignore: error TS2551: Property 'shortcutListModel_' does not
          // exist on type 'permutedHandler'. Did you mean 'shortcutList_'?
          const shortcutEntry = this.shortcutListModel_.item(modelIndex);
          newList.push(entryToModelItem(shortcutEntry));
        }

        // Fill remaining permutation if necessary.
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        for (; oldListIndex < this.shortcutList_.length; oldListIndex++) {
          permutation.push(-1);
        }

        this.shortcutList_ = newList;
      } else if (listType == ListType.ANDROID_APP_LIST) {
        this.androidAppList_ = [];
        // @ts-ignore: error TS2551: Property 'androidAppListModel_' does not
        // exist on type 'permutedHandler'. Did you mean 'androidAppList_'?
        for (let i = 0; i < this.androidAppListModel_.length(); i++) {
          // @ts-ignore: error TS2551: Property 'androidAppListModel_' does not
          // exist on type 'permutedHandler'. Did you mean 'androidAppList_'?
          this.androidAppList_.push(this.androidAppListModel_.item(i));
        }
      }

      // Reorder items after permutation.
      // @ts-ignore: error TS2339: Property 'refreshNavigationItems' does not
      // exist on type 'permutedHandler'.
      this.refreshNavigationItems();

      // Dispatch permuted event.
      const permutedEvent = new Event('permuted');
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      permutedEvent.newLength = this.volumeList_.length +
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          this.shortcutList_.length + this.androidAppList_.length;
      // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
      // 'Event'.
      permutedEvent.permutation = permutation;
      // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on
      // type 'permutedHandler'.
      this.dispatchEvent(permutedEvent);
    };

    this.volumeManager_.volumeInfoList.addEventListener(
        'permuted', permutedHandler.bind(this, ListType.VOLUME_LIST));
    this.shortcutListModel_.addEventListener(
        'permuted', permutedHandler.bind(this, ListType.SHORTCUT_LIST));
    this.androidAppListModel_.addEventListener(
        'permuted', permutedHandler.bind(this, ListType.ANDROID_APP_LIST));

    // 'change' event is just ignored, because it is not fired neither in
    // the folder shortcut list nor in the volume info list.
    // 'splice' and 'sorted' events are not implemented, since they are not used
    // in list.js.
  }

  get length() {
    return this.length_();
  }

  get folderShortcutList() {
    return this.shortcutList_;
  }

  /**
   * Set the crostini Linux files root and reorder items.
   * @param {NavigationModelFakeItem} item Linux files root.
   */
  set linuxFilesItem(item) {
    this.linuxFilesItem_ = item;
    this.refreshNavigationItems();
  }

  /**
   * Set the Guest OS Files placeholder roots and reorder items.
   * @param {!Array<!NavigationModelFakeItem>} items Guest OS Files roots.
   */
  set guestOsPlaceholders(items) {
    this.guestOsPlaceholders_ = items;
    this.refreshNavigationItems();
  }

  /**
   * Set the fake Drive root and reorder items.
   * @param {?NavigationModelFakeItem} item Fake Drive root.
   */
  set fakeDriveItem(item) {
    this.fakeDriveItem_ = item;
    this.refreshNavigationItems();
  }

  /**
   * Set the fake Trash root and reorder items.
   * @param {?NavigationModelFakeItem} item Fake Trash root.
   */
  set fakeTrashItem(item) {
    this.trashItem_ = item;
    this.refreshNavigationItems();
  }

  /**
   * Refresh list of navigation items.
   */
  refreshNavigationItems() {
    return this.orderAndNestItems_();
  }

  /**
   * Reorder navigation items and nest some within "Downloads"
   * which will be displayed as "My-Files". Desired order:
   *  1. Recents.
   *  2. Media Views (Images, Videos and Audio).
   *  3. Shortcuts.
   *  4. "My-Files" (grouping), actually Downloads volume.
   *    4.1. Downloads
   *    4.2. Play files (android volume) (if enabled).
   *    4.3. Linux files (crostini volume or fake item) (if enabled).
   *  5. Google Drive.
   *  6. ODFS.
   *  7. SMBs.
   *  8. Other FSP (File System Provider) (when mounted).
   *  9. Other volumes (MTP, ARCHIVE, REMOVABLE).
   *  10. Add new services if (it exists).
   * @private
   */
  orderAndNestItems_() {
    const volumeIndexes = {};
    const volumeList = this.volumeList_;

    // Find the index of each volumeType from the array volumeList_,
    // for volumes that can have multiple entries it saves as list
    // of indexes, otherwise saves the index as int directly.
    for (let i = 0; i < volumeList.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      const volumeType = volumeList[i].volumeInfo.volumeType;
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      volumeList[i].originalOrder = i;
      // @ts-ignore: error TS6133: 'providedType' is declared but its value is
      // never read.
      let providedType;
      // @ts-ignore: error TS6133: 'volumeId' is declared but its value is never
      // read.
      let volumeId;
      switch (volumeType) {
        case VolumeType.CROSTINI:
        case VolumeType.DOWNLOADS:
        case VolumeType.ANDROID_FILES:
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'string' can't be used to index type
          // '{}'.
          volumeIndexes[volumeType] = i;
          break;
        case VolumeType.PROVIDED:
        case VolumeType.REMOVABLE:
        case VolumeType.ARCHIVE:
        case VolumeType.MTP:
        case VolumeType.DRIVE:
        case VolumeType.MEDIA_VIEW:
        case VolumeType.DOCUMENTS_PROVIDER:
        case VolumeType.SMB:
        case VolumeType.GUEST_OS:
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'string' can't be used to index type
          // '{}'.
          if (!volumeIndexes[volumeType]) {
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
            volumeIndexes[volumeType] = [i];
          } else {
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
            volumeIndexes[volumeType].push(i);
          }
          break;
        default:
          assertNotReached(`No explict order for VolumeType: "${volumeType}"`);
          break;
      }
    }

    /**
     * @param {!VolumeType} volumeType the desired volume
     *     type to be filtered from volumeList.
     * @return {NavigationModelVolumeItem}
     */
    const getSingleVolume = volumeType => {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      return volumeList[volumeIndexes[volumeType]];
    };

    /**
     * @param {!VolumeType} volumeType the desired volume
     *     type to be filtered from volumeList.
     * @return Array<!NavigationModelVolumeItem>
     */
    const getVolumes = volumeType => {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      const indexes = volumeIndexes[volumeType] || [];
      // @ts-ignore: error TS7006: Parameter 'idx' implicitly has an 'any' type.
      return indexes.map(idx => volumeList[idx]);
    };

    /**
     * Removable volumes which share the same device path (i.e. partitions) are
     * grouped.
     * @return !Map<string, !Array<!NavigationModelVolumeItem>>
     */
    const groupRemovables = () => {
      const removableGroups = new Map();
      const removableVolumes = getVolumes(VolumeType.REMOVABLE);

      for (const removable of removableVolumes) {
        // Partitions on the same physical device share device path and drive
        // label. Create keys using these two identifiers.
        const key = removable.volumeInfo.devicePath + '/' +
            removable.volumeInfo.driveLabel;
        if (!removableGroups.has(key)) {
          // New key, so create a new array to hold partitions.
          removableGroups.set(key, []);
        }
        // Add volume to array of volumes matching device path and drive label.
        removableGroups.get(key).push(removable);
      }

      return removableGroups;
    };

    // Items as per required order.
    this.navigationItems_ = [];

    if (this.recentModelItem_) {
      this.navigationItems_.push(this.recentModelItem_);
    }

    // Shortcuts.
    for (const shortcut of this.shortcutList_) {
      this.navigationItems_.push(shortcut);
    }

    let myFilesEntry;
    let myFilesModel;
    if (!this.myFilesModel_) {
      // When MyFilesVolume is enabled we use the Downloads volume to be the
      // MyFiles volume.
      const myFilesVolumeModel = getSingleVolume(VolumeType.DOWNLOADS);
      if (myFilesVolumeModel) {
        myFilesEntry = new VolumeEntry(myFilesVolumeModel.volumeInfo);
        myFilesModel = new NavigationModelFakeItem(
            str('MY_FILES_ROOT_LABEL'), NavigationModelItemType.ENTRY_LIST,
            myFilesEntry);
        myFilesModel.section = NavigationSection.MY_FILES;
        this.myFilesModel_ = myFilesModel;
      } else {
        // When MyFiles volume isn't available we create a empty EntryList to
        // be MyFiles to be able to display Linux or Play volumes. However we
        // don't save it back to this.MyFilesModel_ so it's always re-created.
        myFilesEntry =
            new EntryList(str('MY_FILES_ROOT_LABEL'), RootType.MY_FILES);
        myFilesModel = new NavigationModelFakeItem(
            myFilesEntry.label, NavigationModelItemType.ENTRY_LIST,
            myFilesEntry);
        myFilesModel.section = NavigationSection.MY_FILES;
      }
    } else {
      myFilesEntry = this.myFilesModel_.entry;
      myFilesModel = this.myFilesModel_;
    }
    this.navigationItems_.push(myFilesModel);

    // Add Android to My Files.
    const androidVolume = getSingleVolume(VolumeType.ANDROID_FILES);
    if (androidVolume) {
      // Only add volume if MyFiles doesn't have it yet.
      // @ts-ignore: error TS2339: Property 'findIndexByVolumeInfo' does not
      // exist on type 'FilesAppEntry | EntryList | VolumeEntry'.
      if (myFilesEntry.findIndexByVolumeInfo(androidVolume.volumeInfo) === -1) {
        const volumeEntry = new VolumeEntry(androidVolume.volumeInfo);
        volumeEntry.disabled =
            this.volumeManager_.isDisabled(VolumeType.ANDROID_FILES);
        // @ts-ignore: error TS2339: Property 'addEntry' does not exist on type
        // 'FilesAppEntry | EntryList | VolumeEntry'.
        myFilesEntry.addEntry(volumeEntry);
      }
    } else {
      // @ts-ignore: error TS2339: Property 'removeByVolumeType' does not exist
      // on type 'FilesAppEntry | EntryList | VolumeEntry'.
      myFilesEntry.removeByVolumeType(VolumeType.ANDROID_FILES);
    }

    // Add Linux to My Files.
    const crostiniVolume = getSingleVolume(VolumeType.CROSTINI);

    // Remove Crostini FakeEntry, it's re-added below if needed.
    // @ts-ignore: error TS2339: Property 'removeAllByRootType' does not exist
    // on type 'FilesAppEntry | EntryList | VolumeEntry'.
    myFilesEntry.removeAllByRootType(RootType.CROSTINI);
    if (crostiniVolume) {
      // Crostini is mounted so add it if MyFiles doesn't have it yet.
      // @ts-ignore: error TS2339: Property 'findIndexByVolumeInfo' does not
      // exist on type 'FilesAppEntry | EntryList | VolumeEntry'.
      if (myFilesEntry.findIndexByVolumeInfo(crostiniVolume.volumeInfo) ===
          -1) {
        const volumeEntry = new VolumeEntry(crostiniVolume.volumeInfo);
        volumeEntry.disabled =
            this.volumeManager_.isDisabled(VolumeType.CROSTINI);
        // @ts-ignore: error TS2339: Property 'addEntry' does not exist on type
        // 'FilesAppEntry | EntryList | VolumeEntry'.
        myFilesEntry.addEntry(volumeEntry);
      }
    } else {
      // @ts-ignore: error TS2339: Property 'removeByVolumeType' does not exist
      // on type 'FilesAppEntry | EntryList | VolumeEntry'.
      myFilesEntry.removeByVolumeType(VolumeType.CROSTINI);
      if (this.linuxFilesItem_) {
        // Here it's just a fake item, we link the navigation model so
        // DirectoryTree can choose the correct DirectoryItem for it.
        // @ts-ignore: error TS2339: Property 'navigationModel' does not exist
        // on type 'FilesAppEntry'.
        this.linuxFilesItem_.entry.navigationModel = this.linuxFilesItem_;
        // @ts-ignore: error TS2339: Property 'addEntry' does not exist on type
        // 'FilesAppEntry | EntryList | VolumeEntry'.
        myFilesEntry.addEntry(this.linuxFilesItem_.entry);
      }
    }

    // TODO(crbug/1293229): To start with, we only support listing and not
    // mounting, which means we're just dealing with fake entries here. This
    // gets updated to handle real volumes once we support mounting them.
    if (isGuestOsEnabled()) {
      // Remove all GuestOs placeholders, we readd any if they're needed.
      // @ts-ignore: error TS2339: Property 'removeAllByRootType' does not exist
      // on type 'FilesAppEntry | EntryList | VolumeEntry'.
      myFilesEntry.removeAllByRootType(RootType.GUEST_OS);

      // For each volume, add any which aren't already in the list.
      let guestOsVolumes = getVolumes(VolumeType.GUEST_OS);
      if (isArcVmEnabled()) {
        // Remove GuestOs Android placeholder, similar to what we did for
        // GuestOs placeholders. This should be readded if needed.
        // @ts-ignore: error TS2339: Property 'removeAllByRootType' does not
        // exist on type 'FilesAppEntry | EntryList | VolumeEntry'.
        myFilesEntry.removeAllByRootType(RootType.ANDROID_FILES);
        const androidVolume = getSingleVolume(VolumeType.ANDROID_FILES);
        if (androidVolume) {
          androidVolume.disabled =
              this.volumeManager_.isDisabled(VolumeType.ANDROID_FILES);
          guestOsVolumes = guestOsVolumes.concat(androidVolume);
        }
      }
      for (const volume of guestOsVolumes) {
        // @ts-ignore: error TS2339: Property 'findIndexByVolumeInfo' does not
        // exist on type 'FilesAppEntry | EntryList | VolumeEntry'.
        if (myFilesEntry.findIndexByVolumeInfo(volume.volumeInfo) === -1) {
          const volumeEntry = new VolumeEntry(volume.volumeInfo);
          volumeEntry.disabled =
              this.volumeManager_.isDisabled(VolumeType.GUEST_OS);
          // @ts-ignore: error TS2339: Property 'addEntry' does not exist on
          // type 'FilesAppEntry | EntryList | VolumeEntry'.
          myFilesEntry.addEntry(volumeEntry);
        }
      }
      // For each entry in the list, remove any for volumes that no longer
      // exist.
      // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
      // type 'FilesAppEntry | EntryList | VolumeEntry'.
      for (const volume of myFilesEntry.getUIChildren()) {
        if (!volume.volumeInfo ||
            volume.volumeInfo.volumeType != VolumeType.GUEST_OS) {
          continue;
        }
        // @ts-ignore: error TS7006: Parameter 'v' implicitly has an 'any' type.
        if (!guestOsVolumes.find(v => v.label === volume.name)) {
          // @ts-ignore: error TS2339: Property 'removeChildEntry' does not
          // exist on type 'FilesAppEntry | EntryList | VolumeEntry'.
          myFilesEntry.removeChildEntry(volume);
        }
      }
      // Now we add any guests we know about which don't already have a
      // matching volume.
      for (const item of this.guestOsPlaceholders_) {
        // @ts-ignore: error TS7006: Parameter 'v' implicitly has an 'any' type.
        if (!guestOsVolumes.find(v => v.label === item.label)) {
          // Since it's a fake item, link the navigation model so
          // DirectoryTree can choose the correct DirectoryItem for it.
          // @ts-ignore: error TS2339: Property 'navigationModel' does not exist
          // on type 'FilesAppEntry'.
          item.entry.navigationModel = item;
          // @ts-ignore: error TS2339: Property 'addEntry' does not exist on
          // type 'FilesAppEntry | EntryList | VolumeEntry'.
          myFilesEntry.addEntry(item.entry);
        }
      }
    }

    // Add Google Drive - the only Drive.
    let hasDrive = false;
    for (const driveItem of getVolumes(VolumeType.DRIVE)) {
      driveItem.disabled = this.volumeManager_.isDisabled(VolumeType.DRIVE);
      this.navigationItems_.push(driveItem);
      driveItem.section = NavigationSection.GOOGLE_DRIVE;
      hasDrive = true;
    }
    if (!hasDrive && this.fakeDriveItem_) {
      this.navigationItems_.push(this.fakeDriveItem_);
      this.fakeDriveItem_.section = NavigationSection.GOOGLE_DRIVE;
    }

    // Add ODFS.
    for (const provided of getVolumes(VolumeType.PROVIDED)) {
      if (isOneDrive(provided.volumeInfo)) {
        provided.section = NavigationSection.ODFS;
        const {volumes} = getStore().getState();
        const volume = volumes[provided.volumeInfo.volumeId];
        // The state might not have been initialized yet. The navigation items
        // will be refreshed once the store gets populated.
        if (volume) {
          provided.disabled = volume.isDisabled;
        }
        this.navigationItems_.push(provided);
      }
    }

    // Add SMB.
    for (const provided of getVolumes(VolumeType.SMB)) {
      this.navigationItems_.push(provided);
      provided.section = NavigationSection.CLOUD;
    }

    // Add other FSPs.
    for (const provided of getVolumes(VolumeType.PROVIDED)) {
      // ODFS added already.
      if (isOneDrive(provided.volumeInfo)) {
        continue;
      }
      this.navigationItems_.push(provided);
      provided.section = NavigationSection.CLOUD;
    }

    // Add DocumentsProviders to the same section of FSP.
    for (const provider of getVolumes(VolumeType.DOCUMENTS_PROVIDER)) {
      this.navigationItems_.push(provider);
      provider.section = NavigationSection.CLOUD;
    }

    // Add REMOVABLE volumes and partitions.
    const removableModels = new Map();
    const disableRemovables =
        this.volumeManager_.isDisabled(VolumeType.REMOVABLE);
    for (const [devicePath, removableGroup] of groupRemovables().entries()) {
      if (removableGroup.length == 1 && !isSinglePartitionFormatEnabled()) {
        // Add unpartitioned removable device as a regular volume.
        this.navigationItems_.push(removableGroup[0]);
        removableGroup[0].section = NavigationSection.REMOVABLE;
        removableGroup[0].disabled = disableRemovables;
        continue;
      }

      // Multiple partitions found.
      let removableModel;
      let removableEntry;
      if (this.removableModels_.has(devicePath)) {
        // Removable model has been seen before. Use the same reference.
        removableModel = this.removableModels_.get(devicePath);
        // @ts-ignore: error TS18048: 'removableModel' is possibly 'undefined'.
        removableModel.disabled = disableRemovables;
        // @ts-ignore: error TS18048: 'removableModel' is possibly 'undefined'.
        removableEntry = removableModel.entry;
      } else {
        // Create an EntryList for new removable group.
        const rootLabel = removableGroup[0].volumeInfo.driveLabel ?
            removableGroup[0].volumeInfo.driveLabel :
            /*default*/ 'External Drive';
        removableEntry = new EntryList(
            rootLabel, RootType.REMOVABLE,
            removableGroup[0].volumeInfo.devicePath);
        removableModel = new NavigationModelFakeItem(
            removableEntry.label, NavigationModelItemType.ENTRY_LIST,
            removableEntry);
        removableModel.disabled = disableRemovables;
        removableModel.section = NavigationSection.REMOVABLE;
      }

      // Remove partitions that aren't available anymore.
      const existingVolumeInfos =
          // @ts-ignore: error TS7006: Parameter 'p' implicitly has an 'any'
          // type.
          new Set(removableGroup.map(p => p.volumeInfo));
      // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
      // type 'FilesAppEntry | EntryList'.
      for (const partition of removableEntry.getUIChildren()) {
        if (!existingVolumeInfos.has(partition.volumeInfo)) {
          // @ts-ignore: error TS2339: Property 'removeChildEntry' does not
          // exist on type 'FilesAppEntry | EntryList'.
          removableEntry.removeChildEntry(partition);
        }
      }

      // Add partitions as entries.
      for (const partition of removableGroup) {
        // Only add partition if it doesn't exist as a child already.
        // @ts-ignore: error TS2339: Property 'findIndexByVolumeInfo' does not
        // exist on type 'FilesAppEntry | EntryList'.
        if (removableEntry.findIndexByVolumeInfo(partition.volumeInfo) === -1) {
          // @ts-ignore: error TS2339: Property 'addEntry' does not exist on
          // type 'FilesAppEntry | EntryList'.
          removableEntry.addEntry(new VolumeEntry(partition.volumeInfo));
        }
      }

      removableModels.set(devicePath, removableModel);
      // @ts-ignore: error TS2345: Argument of type 'NavigationModelFakeItem |
      // undefined' is not assignable to parameter of type
      // 'NavigationModelItem'.
      this.navigationItems_.push(removableModel);
    }
    this.removableModels_ = removableModels;

    // Join MTP, ARCHIVE. These types belong to same section.
    // @ts-ignore: error TS7005: Variable 'otherVolumes' implicitly has an
    // 'any[]' type.
    const otherVolumes =
        [].concat(getVolumes(VolumeType.ARCHIVE), getVolumes(VolumeType.MTP))
            .sort((volume1, volume2) => {
              // @ts-ignore: error TS2339: Property 'originalOrder' does not
              // exist on type 'never'.
              return volume1.originalOrder - volume2.originalOrder;
            });

    for (const volume of otherVolumes) {
      this.navigationItems_.push(volume);
      volume.section = NavigationSection.REMOVABLE;
    }

    for (const androidApp of this.androidAppList_) {
      const androidAppItem = new NavigationModelAndroidAppItem(androidApp);
      androidAppItem.section = NavigationSection.ANDROID_APPS;
      this.navigationItems_.push(androidAppItem);
    }

    // Add Trash.
    // This should only show when Files app is open as a standalone app. The ARC
    // file selector, however, opens Files app as a standalone app but passes a
    // query parameter to indicate the mode. As Trash is a fake volume, it is
    // not filtered out in the filtered volume manager so perform it here
    // instead.
    if (this.dialogType_ === DialogType.FULL_PAGE &&
        !this.volumeManager_.getMediaStoreFilesOnlyFilterEnabled() &&
        this.trashItem_) {
      this.navigationItems_.push(this.trashItem_);
    }
  }

  /**
   * Returns the item at the given index.
   * @param {number} index The index of the entry to get.
   * @return {NavigationModelItem|undefined} The item at the given index.
   */
  item(index) {
    return this.navigationItems_[index];
  }

  /**
   * Returns the number of items in the model.
   * @return {number} The length of the model.
   * @private
   */
  length_() {
    return this.navigationItems_.length;
  }

  /**
   * Returns the first matching item.
   * @param {NavigationModelItem} modelItem The entry to find.
   * @param {number=} opt_fromIndex If provided, then the searching start at
   *     the {@code opt_fromIndex}.
   * @return {number} The index of the first found element or -1 if not found.
   */
  indexOf(modelItem, opt_fromIndex) {
    for (let i = opt_fromIndex || 0; i < this.length; i++) {
      if (modelItem === this.item(i)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Called externally when one of the items is not found on the filesystem.
   * @param {!NavigationModelItem} modelItem The entry which is not found.
   */
  onItemNotFoundError(modelItem) {
    if (modelItem.type === NavigationModelItemType.SHORTCUT) {
      this.shortcutListModel_.onItemNotFoundError(
          /** @type {!NavigationModelShortcutItem} */ (modelItem).entry);
    }
  }

  /**
   * Get the index of Downloads volume in the volume list. Returns -1 if there
   * is not the Downloads volume in the list.
   * @returns {number} Index of the Downloads volume.
   */
  findDownloadsVolumeIndex_() {
    for (let i = 0; i < this.volumeList_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (this.volumeList_[i].volumeInfo.volumeType == VolumeType.DOWNLOADS) {
        return i;
      }
    }
    return -1;
  }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @enum {string}
 */
var NavigationModelItemType = {
  SHORTCUT: 'shortcut',
  VOLUME: 'volume',
  RECENT: 'recent',
  CROSTINI: 'crostini',
  ENTRY_LIST: 'entry-list',
};

/**
 * Navigation List sections. A section is just a visual grouping of some items.
 *
 * Sections:
 *      - TOP: Recents, Shortcuts.
 *      - MY_FILES: My Files (which includes Downloads, Crostini and Arc++ as
 *                  its children).
 *      - REMOVABLE: Archives, MTPs, Media Views and Removables.
 *      - CLOUD: Drive and FSPs.
 * @enum {string}
 */
var NavigationSection = {
  TOP: 'top',
  MY_FILES: 'my_files',
  REMOVABLE: 'removable',
  CLOUD: 'cloud',
};

/**
 * Base item of NavigationListModel. Should not be created directly.
 * @param {string} label
 * @param {NavigationModelItemType} type
 * @constructor
 * @struct
 */
function NavigationModelItem(label, type) {
  this.label_ = label;
  this.type_ = type;

  /**
   * @type {NavigationSection} section which this item belongs to.
   */
  this.section_ = NavigationSection.TOP;

  /** @type {number} original order when returned from VolumeManager. */
  this.originalOrder_ = -1;
}

NavigationModelItem.prototype = /** @struct */ {
  get label() {
    return this.label_;
  },
  get type() {
    return this.type_;
  },

  /** @type {NavigationSection} */
  get section() {
    return this.section_;
  },
  /** @param {NavigationSection} section */
  set section(section) {
    this.section_ = section;
  },

  /** @type {number} */
  get originalOrder() {
    return this.originalOrder_;
  },
  set originalOrder(order) {
    this.originalOrder_ = order;
  },
};

/**
 * Item of NavigationListModel for shortcuts.
 *
 * @param {string} label Label.
 * @param {!DirectoryEntry} entry Entry. Cannot be null.
 * @constructor
 * @extends {NavigationModelItem}
 * @struct
 */
function NavigationModelShortcutItem(label, entry) {
  NavigationModelItem.call(this, label, NavigationModelItemType.SHORTCUT);
  this.entry_ = entry;
}

NavigationModelShortcutItem.prototype = /** @struct */ {
  __proto__: NavigationModelItem.prototype,
  get entry() { return this.entry_; }
};

/**
 * Item of NavigationListModel for volumes.
 *
 * @param {string} label Label.
 * @param {!VolumeInfo} volumeInfo Volume info for the volume. Cannot be null.
 * @constructor
 * @extends {NavigationModelItem}
 */
function NavigationModelVolumeItem(label, volumeInfo) {
  NavigationModelItem.call(this, label, NavigationModelItemType.VOLUME);
  this.volumeInfo_ = volumeInfo;
  // Start resolving the display root because it is used
  // for determining executability of commands.
  this.volumeInfo_.resolveDisplayRoot(
      function() {}, function() {});
}

NavigationModelVolumeItem.prototype = /** @struct */ {
  __proto__: NavigationModelItem.prototype,
  get volumeInfo() { return this.volumeInfo_; }
};

/**
 * Item of NavigationListModel for a fake item such as Recent or Linux files.
 *
 * @param {string} label Label on the menu button.
 * @param {NavigationModelItemType} type
 * @param {!FilesAppEntry} entry Fake entry for the root folder.
 * @constructor
 * @extends {NavigationModelItem}
 * @struct
 */
function NavigationModelFakeItem(label, type, entry) {
  NavigationModelItem.call(this, label, type);
  this.entry_ = entry;
}

NavigationModelFakeItem.prototype = /** @struct */ {
  __proto__: NavigationModelItem.prototype,
  get entry() {
    return this.entry_;
  }
};

/**
 * A navigation list model. This model combines multiple models.
 * @param {!VolumeManager} volumeManager VolumeManager instance.
 * @param {(!cr.ui.ArrayDataModel|!FolderShortcutsDataModel)} shortcutListModel
 *     The list of folder shortcut.
 * @param {NavigationModelFakeItem} recentModelItem Recent folder.
 * @constructor
 * @extends {cr.EventTarget}
 */
function NavigationListModel(
    volumeManager, shortcutListModel, recentModelItem) {
  cr.EventTarget.call(this);

  /**
   * @private {!VolumeManager}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {(!cr.ui.ArrayDataModel|!FolderShortcutsDataModel)}
   * @const
   */
  this.shortcutListModel_ = shortcutListModel;

  /**
   * @private {NavigationModelFakeItem}
   * @const
   */
  this.recentModelItem_ = recentModelItem;

  /**
   * Root folder for crostini Linux files.
   * This field will be set asynchronously after calling
   * chrome.fileManagerPrivate.isCrostiniEnabled.
   * @private {NavigationModelFakeItem}
   */
  this.linuxFilesItem_ = null;

  /**
   * NavigationModel for MyFiles, since DirectoryTree expect it to be always the
   * same reference we keep the initial reference for reuse.
   * @private {NavigationModelFakeItem}
   */
  this.myFilesModel_ = null;

  /**
   * All root navigation items in display order.
   * @private {!Array<!NavigationModelItem>}
   */
  this.navigationItems_ = [];

  var volumeInfoToModelItem = function(volumeInfo) {
    return new NavigationModelVolumeItem(
        volumeInfo.label,
        volumeInfo);
  }.bind(this);

  var entryToModelItem = function(entry) {
    var item = new NavigationModelShortcutItem(
        entry.name,
        entry);
    return item;
  }.bind(this);

  /**
   * Type of updated list.
   * @enum {number}
   * @const
   */
  var ListType = {
    VOLUME_LIST: 1,
    SHORTCUT_LIST: 2
  };
  Object.freeze(ListType);

  // Generates this.volumeList_ and this.shortcutList_ from the models.
  this.volumeList_ = [];
  for (let i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
    this.volumeList_.push(
        volumeInfoToModelItem(this.volumeManager_.volumeInfoList.item(i)));
  }

  this.shortcutList_ = [];
  for (var i = 0; i < this.shortcutListModel_.length; i++) {
    var shortcutEntry = /** @type {!Entry} */ (this.shortcutListModel_.item(i));
    var volumeInfo = this.volumeManager_.getVolumeInfo(shortcutEntry);
    this.shortcutList_.push(entryToModelItem(shortcutEntry));
  }

  // Reorder volumes, shortcuts, and optional items for initial display.
  this.reorderNavigationItems_();

  // Generates a combined 'permuted' event from an event of either volumeList or
  // shortcutList.
  var permutedHandler = function(listType, event) {
    var permutation;

    // Build the volumeList.
    if (listType == ListType.VOLUME_LIST) {
      // The volume is mounted or unmounted.
      var newList = [];

      // Use the old instances if they just move.
      for (var i = 0; i < event.permutation.length; i++) {
        if (event.permutation[i] >= 0)
          newList[event.permutation[i]] = this.volumeList_[i];
      }

      // Create missing instances.
      for (var i = 0; i < event.newLength; i++) {
        if (!newList[i]) {
          newList[i] = volumeInfoToModelItem(
              this.volumeManager_.volumeInfoList.item(i));
        }
      }
      this.volumeList_ = newList;

      permutation = event.permutation.slice();

      // shortcutList part has not been changed, so the permutation should be
      // just identity mapping with a shift.
      for (var i = 0; i < this.shortcutList_.length; i++) {
        permutation.push(i + this.volumeList_.length);
      }
    } else {
      // Build the shortcutList.

      // volumeList part has not been changed, so the permutation should be
      // identity mapping.

      permutation = [];
      for (var i = 0; i < this.volumeList_.length; i++) {
        permutation[i] = i;
      }

      var modelIndex = 0;
      var oldListIndex = 0;
      var newList = [];
      while (modelIndex < this.shortcutListModel_.length &&
             oldListIndex < this.shortcutList_.length) {
        var shortcutEntry = this.shortcutListModel_.item(modelIndex);
        var cmp = this.shortcutListModel_.compare(
            /** @type {Entry} */ (shortcutEntry),
            this.shortcutList_[oldListIndex].entry);
        if (cmp > 0) {
          // The shortcut at shortcutList_[oldListIndex] is removed.
          permutation.push(-1);
          oldListIndex++;
          continue;
        }

        if (cmp === 0) {
          // Reuse the old instance.
          permutation.push(newList.length + this.volumeList_.length);
          newList.push(this.shortcutList_[oldListIndex]);
          oldListIndex++;
        } else {
          // We needs to create a new instance for the shortcut entry.
          newList.push(entryToModelItem(shortcutEntry));
        }
        modelIndex++;
      }

      // Add remaining (new) shortcuts if necessary.
      for (; modelIndex < this.shortcutListModel_.length; modelIndex++) {
        var shortcutEntry = this.shortcutListModel_.item(modelIndex);
        newList.push(entryToModelItem(shortcutEntry));
      }

      // Fill remaining permutation if necessary.
      for (; oldListIndex < this.shortcutList_.length; oldListIndex++)
        permutation.push(-1);

      this.shortcutList_ = newList;
    }

    // Reorder items after permutation.
    this.reorderNavigationItems_();

    // Dispatch permuted event.
    var permutedEvent = new Event('permuted');
    permutedEvent.newLength =
        this.volumeList_.length + this.shortcutList_.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);
  };

  this.volumeManager_.volumeInfoList.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.VOLUME_LIST));
  this.shortcutListModel_.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.SHORTCUT_LIST));

  // 'change' event is just ignored, because it is not fired neither in
  // the folder shortcut list nor in the volume info list.
  // 'splice' and 'sorted' events are not implemented, since they are not used
  // in list.js.
}

/**
 * ZipArchiver mounts zip files as PROVIDED volume type.
 * This is a special case for zip volumes to be able to split them apart from
 * PROVIDED.
 * @const
 */
NavigationListModel.ZIP_VOLUME_TYPE = '_ZIP_VOLUME_';

/**
 * Id of the Zip Archiver extension, that can mount zip files.
 * @const
 */
NavigationListModel.ZIP_EXTENSION_ID = 'dmboannefpncccogfdikhmhpmdnddgoe';

/**
 * NavigationList inherits cr.EventTarget.
 */
NavigationListModel.prototype = {
  __proto__: cr.EventTarget.prototype,
  get length() {
    return this.length_();
  },
  get folderShortcutList() {
    return this.shortcutList_;
  },
  /**
   * Set the crostini Linux files root and reorder items.
   * This setter is provided separate to the constructor since
   * this field is set async after calling fileManagerPrivate.isCrostiniEnabled.
   * @param {NavigationModelFakeItem} item Linux files root.
   */
  set linuxFilesItem(item) {
    this.linuxFilesItem_ = item;
    this.reorderNavigationItems_();
  },
};

/**
 * Reorder navigation items when command line flag new-files-app-navigation is
 * enabled it nests Downloads, Linux and Android files under "My Files"; when
 * it's disabled it has a flat structure with Linux files after Recent menu.
 */
NavigationListModel.prototype.reorderNavigationItems_ = function() {
  return this.orderAndNestItems_();
};

/**
 * Reorder navigation items in the following order:
 *  1. Volumes.
 *  2. If Downloads exists, then immediately after Downloads should be:
 *  2a. Recent if it exists.
 *  2b. Linux files if it exists and is not mounted.
 *      When mounted, it will be located in Volumes at this position.
 *  3. Shortcuts.
 *  4. Add new services if it exists.
 * @private
 */
NavigationListModel.prototype.flatNavigationItems_ = function() {
  // Check if Linux files already mounted.
  let linuxFilesMounted = false;
  for (let i = 0; i < this.volumeList_.length; i++) {
    if (this.volumeList_[i].volumeInfo.volumeType ===
        VolumeManagerCommon.VolumeType.CROSTINI) {
      linuxFilesMounted = true;
      break;
    }
  }

  // Items as per required order.
  this.navigationItems_ = this.volumeList_.slice();
  var downloadsVolumeIndex = this.findDownloadsVolumeIndex_();
  if (this.linuxFilesItem_ && !linuxFilesMounted && downloadsVolumeIndex >= 0)
    this.navigationItems_.splice(
        downloadsVolumeIndex + 1, 0, this.linuxFilesItem_);
  if (this.recentModelItem_ && downloadsVolumeIndex >= 0)
    this.navigationItems_.splice(
        downloadsVolumeIndex + 1, 0, this.recentModelItem_);
  Array.prototype.push.apply(this.navigationItems_, this.shortcutList_);
  if (this.addNewServicesItem_)
    this.navigationItems_.push(this.addNewServicesItem_);
};

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
 *  5. Drive volumes.
 *  6. Other FSP (File System Provider) (when mounted).
 *  7. Other volumes (MTP, ARCHIVE, REMOVABLE, Zip volumes).
 *  8. Add new services if (it exists).
 * @private
 */
NavigationListModel.prototype.orderAndNestItems_ = function() {
  const volumeIndexes = {};
  const volumeList = this.volumeList_;

  // Find the index of each volumeType from the array volumeList_,
  // for volumes that can have multiple entries it saves as list
  // of indexes, otherwise saves the index as int directly.
  for (let i = 0; i < volumeList.length; i++) {
    const volumeType = volumeList[i].volumeInfo.volumeType;
    volumeList[i].originalOrder = i;
    let providedType;
    let volumeId;
    switch (volumeType) {
      case VolumeManagerCommon.VolumeType.CROSTINI:
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
      case VolumeManagerCommon.VolumeType.ANDROID_FILES:
        volumeIndexes[volumeType] = i;
        break;
      case VolumeManagerCommon.VolumeType.PROVIDED:
        // ZipArchiver mounts zip files as PROVIDED volume type, however we
        // want to display mounted zip files the same way as archive, so
        // splitting them apart from PROVIDED.
        volumeId = volumeList[i].volumeInfo.volumeId;
        providedType = VolumeManagerCommon.VolumeType.PROVIDED;
        if (volumeId.includes(NavigationListModel.ZIP_EXTENSION_ID))
          providedType = NavigationListModel.ZIP_VOLUME_TYPE;
        if (!volumeIndexes[providedType]) {
          volumeIndexes[providedType] = [i];
        } else {
          volumeIndexes[providedType].push(i);
        }
        break;
      case VolumeManagerCommon.VolumeType.REMOVABLE:
      case VolumeManagerCommon.VolumeType.ARCHIVE:
      case VolumeManagerCommon.VolumeType.MTP:
      case VolumeManagerCommon.VolumeType.DRIVE:
      case VolumeManagerCommon.VolumeType.MEDIA_VIEW:
        if (!volumeIndexes[volumeType]) {
          volumeIndexes[volumeType] = [i];
        } else {
          volumeIndexes[volumeType].push(i);
        }
        break;
      default:
        assertNotReached(`No explict order for VolumeType: "${volumeType}"`);
        break;
    }
  }

  /**
   * @param {!VolumeManagerCommon.VolumeType} volumeType the desired volume type
   * to be filtered from volumeList.
   * @return {NavigationModelVolumeItem}
   */
  const getSingleVolume = function(volumeType) {
    return volumeList[volumeIndexes[volumeType]];
  };

  /**
   * @param {!VolumeManagerCommon.VolumeType} volumeType the desired volume type
   * to be filtered from volumeList.
   * @return Array<!NavigationModelVolumeItem>
   */
  const getVolumes = function(volumeType) {
    const indexes = volumeIndexes[volumeType] || [];
    return indexes.map(idx => volumeList[idx]);
  };

  // Items as per required order.
  this.navigationItems_ = [];

  if (this.recentModelItem_)
    this.navigationItems_.push(this.recentModelItem_);

  // Media View (Images, Videos and Audio).
  for (const mediaView of getVolumes(
           VolumeManagerCommon.VolumeType.MEDIA_VIEW)) {
    this.navigationItems_.push(mediaView);
    mediaView.section = NavigationSection.TOP;
  }
  // Shortcuts.
  for (const shortcut of this.shortcutList_)
    this.navigationItems_.push(shortcut);

  let myFilesEntry, myFilesModel;
  if (!this.myFilesModel_) {
    myFilesEntry = new EntryList(
        str('MY_FILES_ROOT_LABEL'), VolumeManagerCommon.RootType.MY_FILES);
    myFilesModel = new NavigationModelFakeItem(
        myFilesEntry.label, NavigationModelItemType.ENTRY_LIST, myFilesEntry);
    myFilesModel.section = NavigationSection.MY_FILES;
    this.myFilesModel_ = myFilesModel;
  } else {
    myFilesEntry = this.myFilesModel_.entry;
    myFilesModel = this.myFilesModel_;
  }
  this.navigationItems_.push(myFilesModel);

  // Add Downloads to My Files.
  const downloadsVolume =
      getSingleVolume(VolumeManagerCommon.VolumeType.DOWNLOADS);
  if (downloadsVolume) {
    // Only add volume if MyFiles doesn't have it yet.
    if (myFilesEntry.findIndexByVolumeInfo(downloadsVolume.volumeInfo) === -1) {
      myFilesEntry.addEntry(new VolumeEntry(downloadsVolume.volumeInfo));
    }
  } else {
    myFilesEntry.removeByVolumeType(VolumeManagerCommon.VolumeType.DOWNLOADS);
  }

  // Add Android to My Files.
  const androidVolume =
      getSingleVolume(VolumeManagerCommon.VolumeType.ANDROID_FILES);
  if (androidVolume) {
    // Only add volume if MyFiles doesn't have it yet.
    if (myFilesEntry.findIndexByVolumeInfo(androidVolume.volumeInfo) === -1) {
      myFilesEntry.addEntry(new VolumeEntry(androidVolume.volumeInfo));
    }
  } else {
    myFilesEntry.removeByVolumeType(
        VolumeManagerCommon.VolumeType.ANDROID_FILES);
  }

  // Add Linux to My Files.
  const crostiniVolume =
      getSingleVolume(VolumeManagerCommon.VolumeType.CROSTINI);

  // Remove Crostini FakeEntry, it's re-added below if needed.
  myFilesEntry.removeByRootType(VolumeManagerCommon.RootType.CROSTINI);
  if (crostiniVolume) {
    // Crostini is mounted so add it if MyFiles doesn't have it yet.
    if (myFilesEntry.findIndexByVolumeInfo(crostiniVolume.volumeInfo) === -1) {
      myFilesEntry.addEntry(new VolumeEntry(crostiniVolume.volumeInfo));
    }
  } else {
    myFilesEntry.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI);
    if (this.linuxFilesItem_) {
      // Here it's just a fake item, we link the navigation model so
      // DirectoryTree can choose the correct DirectoryItem for it.
      this.linuxFilesItem_.entry.navigationModel = this.linuxFilesItem_;
      myFilesEntry.addEntry(this.linuxFilesItem_.entry);
    }
  }

  // Add Drive.
  for (const driveItem of getVolumes(VolumeManagerCommon.VolumeType.DRIVE)) {
    this.navigationItems_.push(driveItem);
    driveItem.section = NavigationSection.CLOUD;
  }

  // Add FSP.
  for (const provided of getVolumes(VolumeManagerCommon.VolumeType.PROVIDED)) {
    this.navigationItems_.push(provided);
    provided.section = NavigationSection.CLOUD;
  }

  // Join MTP, ARCHIVE and REMOVABLE. These types belong to same section.
  const zipIndexes = volumeIndexes[NavigationListModel.ZIP_VOLUME_TYPE] || [];
  const otherVolumes =
      [].concat(
            getVolumes(VolumeManagerCommon.VolumeType.REMOVABLE),
            getVolumes(VolumeManagerCommon.VolumeType.ARCHIVE),
            getVolumes(VolumeManagerCommon.VolumeType.MTP),
            zipIndexes.map(idx => volumeList[idx]))
          .sort((volume1, volume2) => {
            return volume1.originalOrder - volume2.originalOrder;
          });

  for (const volume of otherVolumes) {
    this.navigationItems_.push(volume);
    volume.section = NavigationSection.REMOVABLE;
  }

  if (this.addNewServicesItem_)
    this.navigationItems_.push(this.addNewServicesItem_);
};

/**
 * Returns the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {NavigationModelItem|undefined} The item at the given index.
 */
NavigationListModel.prototype.item = function(index) {
  return this.navigationItems_[index];
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
NavigationListModel.prototype.length_ = function() {
  return this.navigationItems_.length;
};

/**
 * Returns the first matching item.
 * @param {NavigationModelItem} modelItem The entry to find.
 * @param {number=} opt_fromIndex If provided, then the searching start at
 *     the {@code opt_fromIndex}.
 * @return {number} The index of the first found element or -1 if not found.
 */
NavigationListModel.prototype.indexOf = function(modelItem, opt_fromIndex) {
  for (var i = opt_fromIndex || 0; i < this.length; i++) {
    if (modelItem === this.item(i))
      return i;
  }
  return -1;
};

/**
 * Called externally when one of the items is not found on the filesystem.
 * @param {!NavigationModelItem} modelItem The entry which is not found.
 */
NavigationListModel.prototype.onItemNotFoundError = function(modelItem) {
  if (modelItem.type ===  NavigationModelItemType.SHORTCUT)
    this.shortcutListModel_.onItemNotFoundError(
        /** @type {!NavigationModelShortcutItem} */(modelItem).entry);
};

/**
 * Get the index of Downloads volume in the volume list. Returns -1 if there is
 * not the Downloads volume in the list.
 * @returns {number} Index of the Downloads volume.
 */
NavigationListModel.prototype.findDownloadsVolumeIndex_ = function() {
  for (var i = 0; i < this.volumeList_.length; i++) {
    if (this.volumeList_[i].volumeInfo.volumeType ==
        VolumeManagerCommon.VolumeType.DOWNLOADS) {
      return i;
    }
  }
  return -1;
};

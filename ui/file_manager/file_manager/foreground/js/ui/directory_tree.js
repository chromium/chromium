// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent, getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {maybeShowTooltip} from '../../../common/js/dom_utils.js';
import {compareLabelAndGroupBottomEntries, compareName, isComputersEntry, isDescendantEntry, isEntryInsideDrive, isOneDrive, isOneDriveId, isRecentRootType, isSameEntry, isSharedDriveEntry} from '../../../common/js/entry_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {isJellyEnabled} from '../../../common/js/flags.js';
import {vmTypeToIconName} from '../../../common/js/icon_util.js';
import {recordEnum, recordInterval, recordSmallCount, recordUserAction, startInterval} from '../../../common/js/metrics.js';
import {getEntryLabel, str, strf} from '../../../common/js/translations.js';
import {iconSetToCSSBackgroundImageValue} from '../../../common/js/util.js';
import {VolumeManagerCommon} from '../../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../../externs/background/file_operation_manager.js';
import {FilesAppDirEntry} from '../../../externs/files_app_entry_interfaces.js';
import {PropStatus, SearchData, SearchLocation, State} from '../../../externs/ts/state.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {getStore} from '../../../state/store.js';
import {constants} from '../constants.js';
import {FileFilter} from '../directory_contents.js';
import {DirectoryModel} from '../directory_model.js';
import {MetadataModel} from '../metadata/metadata_model.js';
import {NavigationListModel, NavigationModelAndroidAppItem, NavigationModelFakeItem, NavigationModelItem, NavigationModelItemType, NavigationModelShortcutItem, NavigationModelVolumeItem, NavigationSection} from '../navigation_list_model.js';

import {Command} from './command.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {Menu} from './menu.js';
import {Tree, TreeItem} from './tree.js';

// Namespace
const directorytree = {};

////////////////////////////////////////////////////////////////////////////////
// DirectoryTreeBase methods

/**
 * Implementation of methods for DirectoryTree and DirectoryItem. These classes
 * inherits Tree/TreeItem so we can't make them inherit this class.
 * Instead, we separate their implementations to this separate object and call
 * it with setting 'this' from DirectoryTree/Item.
 */
const DirectoryItemTreeBaseMethods = {};

/**
 * Finds an item by entry and returns it.
 * @param {!Entry} entry
 * @return {DirectoryItem} null is returned if it's not found.
 * @this {(DirectoryItem|DirectoryTree)}
 */
DirectoryItemTreeBaseMethods.getItemByEntry = function(entry) {
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'DirectoryItem | DirectoryTree'.
  for (let i = 0; i < this.items.length; i++) {
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem | DirectoryTree'.
    const item = this.items[i];
    if (!item.entry) {
      continue;
    }
    if (isSameEntry(item.entry, entry)) {
      // The Drive root volume item "Google Drive" and its child "My Drive" have
      // the same entry. When we look for a tree item of Drive's root directory,
      // "My Drive" should be returned, as we use "Google Drive" for grouping
      // "My Drive", "Shared with me", "Recent", and "Offline".
      // Therefore, we have to skip "Google Drive" here.
      if (item instanceof DriveVolumeItem) {
        return item.getItemByEntry(entry);
      }

      return item;
    }
    // Team drives are descendants of the Drive root volume item "Google Drive".
    // When we looking for an item in team drives, recursively search inside the
    // "Google Drive" root item.
    if (isSharedDriveEntry(entry) && item instanceof DriveVolumeItem) {
      return item.getItemByEntry(entry);
    }

    if (isComputersEntry(entry) && item instanceof DriveVolumeItem) {
      return item.getItemByEntry(entry);
    }

    if (isDescendantEntry(item.entry, entry)) {
      return item.getItemByEntry(entry);
    }
  }
  // @ts-ignore: error TS2322: Type 'null' is not assignable to type
  // 'DirectoryItem'.
  return null;
};

/**
 * Finds a parent directory of the {@code entry} in {@code this}, and
 * invokes the DirectoryItem.selectByEntry() of the found directory.
 *
 * @param {!DirectoryEntry|!FilesAppDirEntry} entry The entry to be searched
 *     for. Can be a fake.
 * @return {!Promise<boolean>} True if the parent item is found.
 * @this {(DirectoryItem|VolumeItem|DirectoryTree)}
 */
DirectoryItemTreeBaseMethods.searchAndSelectByEntry = async function(entry) {
  // @ts-ignore: error TS2339: Property 'items' does not exist on type
  // 'DirectoryItem | DirectoryTree | VolumeItem'.
  for (let i = 0; i < this.items.length; i++) {
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem | DirectoryTree | VolumeItem'.
    const item = this.items[i];
    if (!item.entry) {
      continue;
    }

    // Team drives are descendants of the Drive root volume item "Google Drive".
    // When we looking for an item in team drives, recursively search inside the
    // "Google Drive" root item.
    if (isSharedDriveEntry(entry) && item instanceof DriveVolumeItem) {
      await item.selectByEntry(entry);
      return true;
    }

    if (isComputersEntry(entry) && item instanceof DriveVolumeItem) {
      await item.selectByEntry(entry);
      return true;
    }

    if (isDescendantEntry(item.entry, entry) ||
        isSameEntry(item.entry, entry)) {
      await item.selectByEntry(entry);
      return true;
    }
  }
  return false;
};

/**
 * Records UMA for the selected entry at {@code location}. Records slightly
 * differently if the expand icon is selected and {@code expandIconSelected} is
 * true.
 *
 * @param {Event} e The click event.
 * @param {VolumeManagerCommon.RootType} rootType The root type to record.
 * @param {boolean} isRootEntry Whether the entry selected was a root entry.
 * @return
 */
DirectoryItemTreeBaseMethods.recordUMASelectedEntry =
    (e, rootType, isRootEntry) => {
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'EventTarget'.
      const expandIconSelected = e.target.classList.contains('expand-icon');
      let metricName = 'Location.OnEntrySelected.TopLevel';
      if (!expandIconSelected && isRootEntry) {
        metricName = 'Location.OnEntrySelected.TopLevel';
      } else if (!expandIconSelected && !isRootEntry) {
        metricName = 'Location.OnEntrySelected.NonTopLevel';
      } else if (expandIconSelected && isRootEntry) {
        metricName = 'Location.OnEntryExpandedOrCollapsed.TopLevel';
      } else if (expandIconSelected && !isRootEntry) {
        metricName = 'Location.OnEntryExpandedOrCollapsed.NonTopLevel';
      }

      recordEnum(metricName, rootType, VolumeManagerCommon.RootTypesForUMA);
    };

Object.freeze(DirectoryItemTreeBaseMethods);

////////////////////////////////////////////////////////////////////////////////
// TreeItem

/**
 * A CSS class .tree-row rowElement contains the content of one tree row, and
 * is always followed by 0 or more children in a 'group' indented by one more
 * level of depth relative their .tree-item parent:
 *
 *   <div class='tree-item'> {class TreeItem extends TreeItem}
 *     <div class='tree-row'>
 *       .tree-row content ...
 *     <div>
 *     <div class='tree-children' role='group' expanded='true||false'>
 *       0 or more indented .tree-item children ...
 *     </div>
 *   </div>
 *
 * Create tree rowElement content: returns a string of HTML used to innerHTML
 * a tree item rowElement.
 * @param {string} id The tree rowElement label Id.
 * @return {string}
 */
directorytree.createRowElementContent = (id) => {
  return `
    <div class='file-row'>
     <span class='expand-icon'></span>
     <span class='icon'></span>
     <span class='label entry-name' id='${id}'></span>
    </div>`;
};

/**
 * Custom tree row style handler: called when the item's |rowElement| should be
 * styled to indent |depth| in the tree for FILES_NG_ENABLED case.
 * @param {!TreeItem} item TreeItem.
 * @param {number} depth Indent depth (>=0).
 */
directorytree.styleRowElementDepth = (item, depth) => {
  // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
  // 'TreeItem'.
  const fileRowElement = item.rowElement.firstElementChild;

  const indent = depth * (isJellyEnabled() ? 20 : 22);
  let style = 'padding-inline-start: ' + indent + 'px';
  const width = indent + 60;
  style += '; min-width: ' + width + 'px;';

  fileRowElement.setAttribute('style', style);
};

/**
 * A tree item has a tree row with a text label.
 */
class FilesTreeItem extends TreeItem {
  /**
   * @param {string} label Label for this item.
   * @param {DirectoryTree} tree Tree that contains this item.
   */
  constructor(label, tree) {
    super();

    // Save the TreeItem label id before overwriting the prototype.
    const id = this.labelElement.id;
    this.__proto__ = FilesTreeItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('entry-label', label);
    }

    this.parentTree_ = tree;

    const innerHTML = directorytree.createRowElementContent(id);
    // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
    // 'FilesTreeItem'.
    this.rowElement.innerHTML =
        sanitizeInnerHtml(innerHTML, {attrs: ['class', 'id']});
    this.label = label;
  }

  /**
   * The element containing the label text.
   * @type {!HTMLElement}
   * @override
   */
  // @ts-ignore: error TS4122: This member cannot have a JSDoc comment with an
  // '@override' tag because it is not declared in the base class 'TreeItem'.
  get labelElement() {
    // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
    // 'FilesTreeItem'.
    return this.rowElement.querySelector('.label');
  }

  /**
   * Updates the expand icon. Defaults to doing nothing for FakeItem and
   * ShortcutItem that don't have children, thus don't need expand icon.
   */
  updateExpandIcon() {}

  /**
   * Change current directory to the entry of this item.
   */
  activate() {}

  /**
   * Invoked when the tree item is clicked.
   *
   * @param {Event} e Click event.
   * @override
   */
  // @ts-ignore: error TS4122: This member cannot have a JSDoc comment with an
  // '@override' tag because it is not declared in the base class 'TreeItem'.
  handleClick(e) {
    // @ts-ignore: error TS2339: Property 'handleClick' does not exist on type
    // 'TreeItem'.
    super.handleClick(e);
    // @ts-ignore: error TS2339: Property 'button' does not exist on type
    // 'Event'.
    if (e.button === 2) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'EventTarget'.
    if (e.target.classList.contains('expand-icon')) {
      return;
    }
    this.activate();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DirectoryItem

/**
 * An expandable directory in the tree. Each element represents one folder (sub
 * directory) or one volume (root directory).
 */
export class DirectoryItem extends FilesTreeItem {
  /**
   * @param {string} label Label for this item.
   * @param {DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(label, tree) {
    super(label, tree);
    this.__proto__ = DirectoryItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'DirectoryItem');
    }

    this.directoryModel_ = tree.directoryModel;
    this.fileFilter_ = tree.directoryModel.getFileFilter();

    // Listen for expand.
    this.addEventListener('expand', this.onExpand_.bind(this), false);

    // Listen for collapse because for the delayed expansion case all
    // children are also collapsed.
    this.addEventListener('collapse', this.onCollapse_.bind(this), false);

    // Default delayExpansion to false. Volumes will set it to true for
    // performance sensitive file systems. SubDirectories will inherit from
    // their parent.
    this.delayExpansion = false;

    // Sets hasChildren=false tentatively. This will be overridden after
    // scanning sub-directories in updateSubElementsFromList().
    this.hasChildren = false;

    // @type {!Array<Entry>} Filled after updateSubDirectories read entries.
    // @ts-ignore: error TS7008: Member 'entries_' implicitly has an 'any[]'
    // type.
    this.entries_ = [];

    // @type {function()=} onMetadataUpdated_ bound to |this| used to listen
    // metadata update events.
    this.onMetadataUpdateBound_ = undefined;
  }

  get typeName() {
    return 'directory_item';
  }

  /**
   * The DirectoryEntry corresponding to this DirectoryItem. This may be
   * a dummy DirectoryEntry.
   * @type {DirectoryEntry|Object}
   */
  get entry() {
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'Object |
    // FileSystemDirectoryEntry'.
    return null;
  }

  /**
   * Gets the RootType of the Volume this entry belongs to.
   * @type {VolumeManagerCommon.RootType|null}
   */
  get rootType() {
    let rootType = null;

    if (this.entry) {
      // @ts-ignore: error TS2345: Argument of type 'Object |
      // FileSystemDirectoryEntry' is not assignable to parameter of type
      // 'FileSystemEntry | FilesAppEntry'.
      const root = this.parentTree_.volumeManager.getLocationInfo(this.entry);
      rootType = root ? root.rootType : null;
    }

    return rootType;
  }

  /**
   * Returns true if this.entry is inside any part of Drive 'My Drive'.
   * @type {!boolean}
   */
  get insideMyDrive() {
    const rootType = this.rootType;
    // @ts-ignore: error TS2322: Type 'string | boolean | null' is not
    // assignable to type 'boolean'.
    return rootType && (rootType === VolumeManagerCommon.RootType.DRIVE);
  }

  /**
   * Returns true if this.entry is inside any part of Drive 'Computers'.
   * @type {!boolean}
   */
  get insideComputers() {
    const rootType = this.rootType;
    // @ts-ignore: error TS2322: Type 'string | boolean | null' is not
    // assignable to type 'boolean'.
    return rootType &&
        (rootType === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         rootType === VolumeManagerCommon.RootType.COMPUTER);
  }

  /**
   * Returns true if this.entry is inside any part of Drive.
   * @type {!boolean}
   */
  get insideDrive() {
    // @ts-ignore: error TS2345: Argument of type '{ rootType: string | null; }'
    // is not assignable to parameter of type 'FileData'.
    return isEntryInsideDrive({rootType: this.rootType});
  }

  /**
   * Returns true if this.entry supports the 'shared' feature, as in, displays
   * a shared icon. It's only supported inside 'My Drive' or 'Computers', even
   * Shared Drive does not support it.
   * @type {!boolean}
   */
  get supportDriveSpecificIcons() {
    return this.insideMyDrive || this.insideComputers;
  }

  /**
   * Handles the Metadata update event. It updates the shared icon of this item
   * sub-folders.
   * @param {Event} event Metadata update event.
   */
  onMetadataUpdated_(event) {
    if (!this.supportDriveSpecificIcons) {
      return;
    }

    const updateableProperties = ['shared', 'isMachineRoot', 'isExternalMedia'];
    // @ts-ignore: error TS2339: Property 'names' does not exist on type
    // 'Event'.
    if (!updateableProperties.some((prop) => event.names.has(prop))) {
      return;
    }

    let index = 0;
    while (this.entries_[index]) {
      const childEntry = this.entries_[index];
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      const childElement = this.items[index];

      // @ts-ignore: error TS2339: Property 'entriesMap' does not exist on type
      // 'Event'.
      if (event.entriesMap.has(childEntry.toURL())) {
        childElement.updateDriveSpecificIcons();
      }

      index++;
    }
  }

  /**
   * Updates sub-elements of {@code this} reading {@code DirectoryEntry}.
   * The list of {@code DirectoryEntry} are not updated by this method.
   *
   * @param {boolean} recursive True if the all visible sub-directories are
   *     updated recursively including left arrows. If false, the update walks
   *     only immediate child directories without arrows.
   * @this {DirectoryItem}
   */
  updateSubElementsFromList(recursive) {
    let index = 0;
    const tree = this.parentTree_;
    let item;

    while (this.entries_[index]) {
      const currentEntry = this.entries_[index];
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      const currentElement = this.items[index];
      const label = getEntryLabel(
                        tree.volumeManager_.getLocationInfo(currentEntry),
                        currentEntry) ||
          '';


      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      if (index >= this.items.length) {
        // If currentEntry carries its navigationModel we generate an item
        // accordingly. Used for Crostini when displayed within My Files.
        if (currentEntry.navigationModel) {
          item = DirectoryTree.createDirectoryItem(
              currentEntry.navigationModel, tree);
        } else {
          item = new SubDirectoryItem(
              label, currentEntry, this, tree, !!currentEntry.disabled);
        }
        // @ts-ignore: error TS2339: Property 'add' does not exist on type
        // 'DirectoryItem'.
        this.add(item);
        index++;
      } else if (isSameEntry(currentEntry, currentElement.entry)) {
        currentElement.updateDriveSpecificIcons();
        if (recursive && this.expanded) {
          if (this.delayExpansion) {
            // Only update deeper on expanded children.
            if (currentElement.expanded) {
              currentElement.updateSubDirectories(true /* recursive */);
            }
            // Show the expander even without knowing if there are children.
            currentElement.mayHaveChildren_ = true;
          } else {
            currentElement.updateExpandIcon();
          }
        }
        index++;
      } else if (currentEntry.toURL() < currentElement.entry.toURL()) {
        // If currentEntry carries its navigationModel we generate an item
        // accordingly. Used for Crostini when displayed within My Files.
        if (currentEntry.navigationModel) {
          item = DirectoryTree.createDirectoryItem(
              currentEntry.navigationModel, tree);
        } else {
          item = new SubDirectoryItem(
              label, currentEntry, this, tree, !!currentEntry.disabled);
        }
        // @ts-ignore: error TS2339: Property 'addAt' does not exist on type
        // 'DirectoryItem'.
        this.addAt(item, index);
        index++;
      } else if (currentEntry.toURL() > currentElement.entry.toURL()) {
        this.remove(currentElement);
      }
    }

    let removedChild;
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem'.
    while (removedChild = this.items[index]) {
      this.remove(removedChild);
    }

    if (index === 0) {
      this.hasChildren = false;
      this.expanded = false;
    } else {
      this.hasChildren = true;
    }
  }

  /**
   * Calls DirectoryItemTreeBaseMethods.getItemByEntry().
   * @param {!Entry} entry
   * @return {DirectoryItem}
   */
  getItemByEntry(entry) {
    return DirectoryItemTreeBaseMethods.getItemByEntry.call(this, entry);
  }

  /**
   * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
   *
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The entry to be searched
   *     for. Can be a fake.
   * @return {!Promise<boolean>} True if the parent item is found.
   */
  async searchAndSelectByEntry(entry) {
    return await DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(
        this, entry);
  }

  /**
   * Overrides WebKit's scrollIntoViewIfNeeded, which doesn't work well with
   * a complex layout. This call is not necessary, so we are ignoring it.
   *
   * @param {boolean=} opt_unused Unused.
   * @override
   */
  // @ts-ignore: error TS6133: 'opt_unused' is declared but its value is never
  // read.
  scrollIntoViewIfNeeded(opt_unused) {}

  /**
   * Removes the child node, but without selecting the parent item, to avoid
   * unintended changing of directories. Removing is done externally, and other
   * code will navigate to another directory.
   *
   * @param {!TreeItem=} child The tree item child to remove.
   * @override
   */
  remove(child) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.lastElementChild.removeChild(/** @type {!TreeItem} */ (child));
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem'.
    if (this.items.length == 0) {
      this.hasChildren = false;
    }
  }

  /**
   * Removes the has-children attribute which allows returning
   * to the ambiguous may-have-children state.
   */
  clearHasChildren() {
    const rowItem = this.firstElementChild;
    this.removeAttribute('has-children');
    // @ts-ignore: error TS18047: 'rowItem' is possibly 'null'.
    rowItem.removeAttribute('has-children');
  }

  /**
   * Invoked when the item is being expanded.
   * @param {!Event} e Event.
   * @private
   */
  onExpand_(e) {
    const rootType = this.rootType;
    const metricName = rootType ? (`DirectoryTree.Expand.${rootType}`) :
                                  'DirectoryTree.Expand.unknown';
    startInterval(metricName);

    if (this.supportDriveSpecificIcons && !this.onMetadataUpdateBound_) {
      this.onMetadataUpdateBound_ = this.onMetadataUpdated_.bind(this);
      this.parentTree_.metadataModel_.addEventListener(
          'update', this.onMetadataUpdateBound_);
    }
    this.updateSubDirectories(
        true /* recursive */,
        () => {
          if (this.insideDrive) {
            this.parentTree_.metadataModel_.get(
                this.entries_,
                constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES
                    .concat(constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES));
          }

          recordInterval(metricName);
        },
        () => {
          this.expanded = false;
          recordInterval(metricName);
        });

    e.stopPropagation();
  }

  /**
   * Invoked when the item is being collapsed.
   * @param {!Event} e Event.
   * @private
   */
  onCollapse_(e) {
    if (this.onMetadataUpdateBound_) {
      this.parentTree_.metadataModel_.removeEventListener(
          'update', this.onMetadataUpdateBound_);
      this.onMetadataUpdateBound_ = undefined;
    }

    if (this.delayExpansion) {
      // For file systems where it is performance intensive
      // to update recursively when items expand this proactively
      // collapses all children to avoid having to traverse large
      // parts of the tree when reopened.
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      for (let i = 0; i < this.items.length; i++) {
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DirectoryItem'.
        const item = this.items[i];

        if (item.expanded) {
          item.expanded = false;
        }
      }
    }

    e.stopPropagation();
  }

  /**
   * Invoked when the tree item is clicked.
   *
   * @param {Event} e Click event.
   * @override
   */
  handleClick(e) {
    super.handleClick(e);

    if (!this.entry) {
      return;
    }

    // If this is DriveVolumeItem, the UMA has already been recorded.
    if (!(this instanceof DriveVolumeItem)) {
      // @ts-ignore: error TS2339: Property 'tree' does not exist on type
      // 'DirectoryItem'.
      const location = this.tree.volumeManager.getLocationInfo(this.entry);
      DirectoryItemTreeBaseMethods.recordUMASelectedEntry.call(
          this, e, location.rootType, location.isRootEntry);
    }
  }

  /**
   * Default sorting for DirectoryItem sub-dirrectories.
   * @param {!Array<!Entry>} entries Entries to be sorted.
   * @returns {!Array<!Entry>}
   */
  sortEntries(entries) {
    entries.sort(compareName);
    const filter = this.fileFilter_.filter.bind(this.fileFilter_);
    return entries.filter(filter);
  }

  /**
   * Retrieves the latest subdirectories and update them on the tree.
   * @param {boolean} recursive True if the update is recursively.
   * @param {function()=} opt_successCallback Callback called on success.
   * @param {function()=} opt_errorCallback Callback called on error.
   */
  updateSubDirectories(recursive, opt_successCallback, opt_errorCallback) {
    // @ts-ignore: error TS2339: Property 'createReader' does not exist on type
    // 'Object | FileSystemDirectoryEntry'.
    if (!this.entry || this.disabled || this.entry.createReader === undefined) {
      opt_errorCallback && opt_errorCallback();
      return;
    }
    // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any'
    // type.
    const onSuccess = (entries) => {
      this.entries_ = entries;
      this.updateSubElementsFromList(recursive);
      opt_successCallback && opt_successCallback();
    };
    // @ts-ignore: error TS2339: Property 'createReader' does not exist on type
    // 'Object | FileSystemDirectoryEntry'.
    const reader = this.entry.createReader();
    // @ts-ignore: error TS7034: Variable 'entries' implicitly has type 'any[]'
    // in some locations where its type cannot be determined.
    const entries = [];
    const readEntry = () => {
      // @ts-ignore: error TS7006: Parameter 'results' implicitly has an 'any'
      // type.
      reader.readEntries((results) => {
        if (!results.length) {
          // @ts-ignore: error TS7005: Variable 'entries' implicitly has an
          // 'any[]' type.
          onSuccess(this.sortEntries(entries));
          return;
        }
        for (let i = 0; i < results.length; i++) {
          const entry = results[i];
          if (entry.isDirectory) {
            entries.push(entry);
          }
        }
        readEntry();
      });
    };
    readEntry();
  }

  /**
   * Updates expand icon.
   * @override
   */
  updateExpandIcon() {
    // @ts-ignore: error TS2339: Property 'createReader' does not exist on type
    // 'Object | FileSystemDirectoryEntry'.
    if (!this.entry || this.disabled || this.entry.createReader === undefined) {
      this.hasChildren = false;
      return;
    }

    // @ts-ignore: error TS2339: Property 'createReader' does not exist on type
    // 'Object | FileSystemDirectoryEntry'.
    const reader = this.entry.createReader();

    const readEntry = () => {
      // @ts-ignore: error TS7006: Parameter 'results' implicitly has an 'any'
      // type.
      reader.readEntries((results) => {
        if (!results.length) {
          // Reached the end without any directory;
          this.hasChildren = false;
          return;
        }

        for (let i = 0; i < results.length; i++) {
          const entry = results[i];
          // If the entry is a directory and is not filtered, the parent
          // directory should be marked as having children
          if (entry.isDirectory && this.fileFilter_.filter(entry)) {
            this.hasChildren = true;
            return;
          }
        }

        // Read next batch of entries.
        readEntry();
      });
    };

    readEntry();
  }

  /**
   * Searches for the changed directory in the current subtree, and if it is
   * found then updates it.
   *
   * @param {!DirectoryEntry} changedDirectoryEntry The entry of the changed
   *     directory.
   */
  updateItemByEntry(changedDirectoryEntry) {
    // @ts-ignore: error TS2345: Argument of type 'Object |
    // FileSystemDirectoryEntry' is not assignable to parameter of type
    // 'FileSystemEntry | FilesAppEntry | null | undefined'.
    if (isSameEntry(changedDirectoryEntry, this.entry)) {
      this.updateSubDirectories(false /* recursive */);
      return;
    }

    // Traverse the entire subtree to find the changed element.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem'.
    for (let i = 0; i < this.items.length; i++) {
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      const item = this.items[i];
      if (!item.entry) {
        continue;
      }
      if (isDescendantEntry(item.entry, changedDirectoryEntry) ||
          isSameEntry(item.entry, changedDirectoryEntry)) {
        item.updateItemByEntry(changedDirectoryEntry);
        break;
      }
    }
  }

  /**
   * Update the icon based on whether the folder is shared on Drive.
   */
  updateDriveSpecificIcons() {}

  /**
   * Select the item corresponding to the given {@code entry}.
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The entry to be selected.
   *     Can be a fake.
   * @return {!Promise<void>}
   */
  async selectByEntry(entry) {
    // @ts-ignore: error TS2345: Argument of type 'Object |
    // FileSystemDirectoryEntry' is not assignable to parameter of type
    // 'FileSystemEntry | FilesAppEntry | null | undefined'.
    if (isSameEntry(entry, this.entry)) {
      this.selected = true;
      return;
    }

    if (await this.searchAndSelectByEntry(entry)) {
      return;
    }

    // If the entry doesn't exist, updates sub directories and tries again.
    await new Promise(
        // @ts-ignore: error TS2345: Argument of type '(opt_successCallback?:
        // (() => any) | undefined, opt_errorCallback?: (() => any) | undefined)
        // => void' is not assignable to parameter of type '(resolve: (value:
        // any) => void, reject: (reason?: any) => void) => void'.
        this.updateSubDirectories.bind(this, false /* recursive */));
    await this.searchAndSelectByEntry(entry);
  }

  /**
   * Executes the assigned action as a drop target.
   */
  doDropTargetAction() {
    this.expanded = true;
  }

  /**
   * Change current directory to the entry of this item.
   * @override
   */
  activate() {
    if (this.entry) {
      // @ts-ignore: error TS2345: Argument of type 'Object |
      // FileSystemDirectoryEntry' is not assignable to parameter of type
      // 'FileSystemDirectoryEntry | FilesAppDirEntry'.
      this.parentTree_.directoryModel.activateDirectoryEntry(this.entry);
    }
  }

  /**
   * Set up eject button. It is placed as the last element of the elements that
   * compose the tree row content.
   * @param {!HTMLElement} rowElement Tree row element.
   * @param {string} targetLabel Label for the ejectable target.
   * @private
   */
  setupEjectButton_(rowElement, targetLabel) {
    const ejectButton = document.createElement('cr-button');

    ejectButton.className = 'root-eject align-right-icon';
    ejectButton.setAttribute(
        'aria-label', strf('UNMOUNT_BUTTON_LABEL', targetLabel));
    ejectButton.setAttribute('tabindex', '0');

    // Block mouse handlers, handle click.
    ejectButton.addEventListener('mouseup', (event) => {
      event.stopPropagation();
    });
    ejectButton.addEventListener('up', (event) => {
      event.stopPropagation();
    });
    ejectButton.addEventListener('mousedown', (event) => {
      event.stopPropagation();
    });
    ejectButton.addEventListener('down', (event) => {
      event.stopPropagation();
    });
    ejectButton.addEventListener('click', (event) => {
      event.stopPropagation();
      const command =
          /** @type {!Command} */ (document.querySelector('command#unmount'));
      // Ensure 'canExecute' state of the command is properly setup for the
      // root before executing it.
      command.canExecuteChange(this);
      command.execute(this);
    });

    // Append eject iron-icon.
    const ironIcon = document.createElement('iron-icon');
    ironIcon.setAttribute('icon', `files20:eject`);
    ejectButton.appendChild(ironIcon);

    // Add the eject button as the last element of the tree row content.
    const label = rowElement.querySelector('.label');
    // @ts-ignore: error TS18047: 'label.parentElement' is possibly 'null'.
    label.parentElement.appendChild(ejectButton);

    // Ensure the eject icon shows when the directory tree is too narrow.
    // @ts-ignore: error TS18047: 'label' is possibly 'null'.
    label.setAttribute('style', 'margin-inline-end: 2px; min-width: 0;');
  }

  /**
   * Set up the context menu for directory items.
   * @param {!Menu} menu Menu to be set.
   * @private
   */
  setContextMenu_(menu) {
    contextMenuHandler.setContextMenu(this, menu);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SubDirectoryItem

/**
 * A subdirectory in the tree. Each element represents a directory that is not
 * a volume's root.
 */
export class SubDirectoryItem extends DirectoryItem {
  /**
   * @param {string} label Label for this item.
   * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
   * @param {DirectoryItem|ShortcutItem|DirectoryTree} parentDirItem
   *     Parent of this item.
   * @param {DirectoryTree} tree Current tree, which contains this item.
   * @param {boolean} disabled Whether this item is disabled. Even if the parent
   *     is not, the subdirectory can be.
   */
  constructor(label, dirEntry, parentDirItem, tree, disabled = false) {
    super(label, tree);
    this.__proto__ = SubDirectoryItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'SubDirectoryItem');
    }

    this.dirEntry_ = dirEntry;
    this.entry = dirEntry;
    this.disabled = disabled;
    // @ts-ignore: error TS2339: Property 'delayExpansion' does not exist on
    // type 'DirectoryItem | DirectoryTree | ShortcutItem'.
    this.delayExpansion = parentDirItem.delayExpansion;

    if (this.delayExpansion) {
      this.clearHasChildren();
      this.mayHaveChildren_ = true;
    }

    // Sets up icons of the item.
    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.add('item-icon');

    // Add volume-dependent attributes / icon.
    const location = tree.volumeManager.getLocationInfo(this.entry);
    if (location && location.rootType && location.isRootEntry) {
      // @ts-ignore: error TS2339: Property 'iconName' does not exist on type
      // 'FileSystemDirectoryEntry'.
      const iconOverride = this.entry.iconName;
      if (iconOverride) {
        // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
        icon.setAttribute('volume-type-icon', iconOverride);
      } else {
        // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
        icon.setAttribute('volume-type-icon', location.rootType);
      }
      if (window.IN_TEST && location.volumeInfo) {
        this.setAttribute(
            'volume-type-for-testing', location.volumeInfo.volumeType);
        // @ts-ignore: error TS2345: Argument of type 'string | undefined' is
        // not assignable to parameter of type 'string'.
        this.setAttribute('drive-label', location.volumeInfo.driveLabel);
      }
    } else {
      const rootType = location && location.rootType ? location.rootType : null;
      // @ts-ignore: error TS2345: Argument of type 'string | null' is not
      // assignable to parameter of type 'string | undefined'.
      const iconOverride = FileType.getIconOverrides(dirEntry, rootType);
      // Add Downloads icon as volume so current test code passes with
      // MyFilesVolume flag enabled and disabled.
      if (iconOverride) {
        // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
        icon.setAttribute('volume-type-icon', iconOverride);
      }
      // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
      icon.setAttribute('file-type-icon', iconOverride || 'folder');
      this.updateDriveSpecificIcons();
    }

    // Setup the item context menu.
    if (tree.contextMenuForSubitems) {
      // @ts-ignore: error TS2341: Property 'setContextMenu_' is private and
      // only accessible within class 'DirectoryItem'.
      this.setContextMenu_(tree.contextMenuForSubitems);
    }

    // Update this directory's expansion icon to reflect if it has children.
    // @ts-ignore: error TS2339: Property 'expanded' does not exist on type
    // 'DirectoryItem | DirectoryTree | ShortcutItem'.
    if (!this.delayExpansion && parentDirItem.expanded) {
      this.updateExpandIcon();
    }
  }

  /**
   * Update the icon based on whether the folder is shared on Drive.
   * @override
   */
  updateDriveSpecificIcons() {
    const metadata = this.parentTree_.metadataModel.getCache(
        [this.dirEntry_], ['shared', 'isMachineRoot', 'isExternalMedia']);

    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.toggle('shared', !!(metadata[0] && metadata[0].shared));

    if (metadata[0] && metadata[0].isMachineRoot) {
      // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
      icon.setAttribute(
          'volume-type-icon', VolumeManagerCommon.RootType.COMPUTER);
    }

    if (metadata[0] && metadata[0].isExternalMedia) {
      // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
      icon.setAttribute(
          'volume-type-icon', VolumeManagerCommon.RootType.EXTERNAL_MEDIA);
    }
  }

  /**
   * The DirectoryEntry corresponding to this DirectoryItem.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'DirectoryItem'.
  get entry() {
    return this.dirEntry_;
  }

  /**
   * Sets the DirectoryEntry corresponding to this DirectoryItem.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'DirectoryItem'.
  set entry(value) {
    this.dirEntry_ = value;

    // Set helper attribute for testing.
    if (window.IN_TEST) {
      this.setAttribute('full-path-for-testing', this.dirEntry_.fullPath);
    }
  }
}

/**
 * A directory of entries. Each element represents an entry.
 */
export class EntryListItem extends DirectoryItem {
  /**
   * @param {VolumeManagerCommon.RootType} rootType The root type to record.
   * @param {!NavigationModelFakeItem} modelItem NavigationModelItem of this
   *     volume.
   * @param {DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(rootType, modelItem, tree) {
    super(modelItem.label, tree);
    this.__proto__ = EntryListItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'EntryListItem');
    }

    this.dirEntry_ = modelItem.entry;
    this.modelItem_ = modelItem;
    this.rootType_ = rootType;
    this.disabled = modelItem.disabled;

    if (rootType === VolumeManagerCommon.RootType.REMOVABLE) {
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'EntryListItem'.
      this.setupEjectButton_(this.rowElement, modelItem.label);

      // For removable add menus for roots to be able to unmount, format, etc.
      if (tree.contextMenuForRootItems) {
        // @ts-ignore: error TS2341: Property 'setContextMenu_' is private and
        // only accessible within class 'DirectoryItem'.
        this.setContextMenu_(tree.contextMenuForRootItems);
      }
    } else {
      // For MyFiles allow normal file operations menus.
      if (tree.contextMenuForSubitems) {
        // @ts-ignore: error TS2341: Property 'setContextMenu_' is private and
        // only accessible within class 'DirectoryItem'.
        this.setContextMenu_(tree.contextMenuForSubitems);
      }
    }

    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.add('item-icon');
    // @ts-ignore: error TS2339: Property 'iconName' does not exist on type
    // 'FileSystemDirectoryEntry'.
    if (this.entry && this.entry.iconName) {
      // @ts-ignore: error TS2339: Property 'iconName' does not exist on type
      // 'FileSystemDirectoryEntry'.
      icon.setAttribute('root-type-icon', this.entry.iconName);
    } else {
      // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
      icon.setAttribute('root-type-icon', rootType);
    }

    // @ts-ignore: error TS2339: Property 'volumeInfo' does not exist on type
    // 'FileSystemDirectoryEntry'.
    if (window.IN_TEST && this.entry && this.entry.volumeInfo) {
      this.setAttribute(
          // @ts-ignore: error TS2339: Property 'volumeInfo' does not exist on
          // type 'FileSystemDirectoryEntry'.
          'volume-type-for-testing', this.entry.volumeInfo.volumeType);
    }

    // MyFiles shows expanded by default.
    if (rootType === VolumeManagerCommon.RootType.MY_FILES) {
      this.mayHaveChildren_ = true;
      this.expanded = true;
    }

    // Update children of this volume.
    this.updateSubDirectories(false /* recursive */);
  }

  /**
   * Default sorting for DirectoryItem sub-dirrectories.
   * @param {!Array<!Entry>} entries Entries to be sorted.
   * @returns {!Array<!Entry>}
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'DirectoryItem'.
  sortEntries(entries) {
    if (!entries.length) {
      return [];
    }

    // If the root entry hasn't been resolved yet.
    if (!this.entry) {
      return DirectoryItem.prototype.sortEntries.apply(this, [entries]);
    }

    // Use locationInfo from first entry because it only compare within the same
    // volume.
    const locationInfo =
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type 'FileSystemEntry |
        // FilesAppEntry'.
        this.parentTree_.volumeManager_.getLocationInfo(entries[0]);
    const compareFunction = compareLabelAndGroupBottomEntries(
        // @ts-ignore: error TS2339: Property 'getUIChildren' does not exist on
        // type 'FileSystemDirectoryEntry'.
        locationInfo, this.entry.getUIChildren());

    const filter = this.fileFilter_.filter.bind(this.fileFilter_);
    return entries.filter(filter).sort(compareFunction);
  }

  /**
   * Retrieves the subdirectories and update them on the tree. Runs
   * synchronously, since EntryList has its subdirectories already in memory.
   * @param {boolean} recursive True if the update is recursively.
   * @param {function()=} opt_successCallback Callback called on success.
   * @param {function()=} opt_errorCallback Callback called on error.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'DirectoryItem'.
  updateSubDirectories(recursive, opt_successCallback, opt_errorCallback) {
    if (!this.entry || this.entry.createReader === undefined) {
      opt_errorCallback && opt_errorCallback();
      return;
    }
    this.entries_ = [];
    // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any'
    // type.
    const onSuccess = (entries) => {
      this.entries_ = entries;
      this.updateSubElementsFromList(recursive);
      opt_successCallback && opt_successCallback();
    };
    const reader = this.entry.createReader();
    // @ts-ignore: error TS7034: Variable 'entries' implicitly has type 'any[]'
    // in some locations where its type cannot be determined.
    const entries = [];
    const readEntry = () => {
      reader.readEntries((results) => {
        if (!results.length) {
          // @ts-ignore: error TS7005: Variable 'entries' implicitly has an
          // 'any[]' type.
          onSuccess(this.sortEntries(entries));
          return;
        }
        for (let i = 0; i < results.length; i++) {
          const entry = results[i];
          // @ts-ignore: error TS18048: 'entry' is possibly 'undefined'.
          if (entry.isDirectory) {
            entries.push(entry);
          }
        }
        readEntry();
      });
    };
    readEntry();
  }

  /**
   * The DirectoryEntry corresponding to this DirectoryItem.
   * @type {DirectoryEntry}
   * @override
   */
  get entry() {
    // @ts-ignore: error TS2739: Type 'FilesAppEntry' is missing the following
    // properties from type 'FileSystemDirectoryEntry': createReader,
    // getDirectory, getFile, removeRecursively
    return this.dirEntry_;
  }

  /**
   * @type {!NavigationModelVolumeItem}
   */
  get modelItem() {
    // @ts-ignore: error TS2739: Type 'NavigationModelFakeItem' is missing the
    // following properties from type 'NavigationModelVolumeItem': volumeInfo_,
    // volumeInfo
    return this.modelItem_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// VolumeItem

/**
 * A TreeItem which represents a volume. Volume items are displayed as
 * top-level children of DirectoryTree.
 */
class VolumeItem extends DirectoryItem {
  /**
   * @param {!NavigationModelVolumeItem} modelItem NavigationModelItem of this
   *     volume.
   * @param {!DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(modelItem, tree) {
    super(modelItem.volumeInfo.label, tree);
    this.__proto__ = VolumeItem.prototype;

    this.modelItem_ = modelItem;
    this.volumeInfo_ = modelItem.volumeInfo;
    this.disabled = modelItem.disabled;

    // Certain (often network) file systems should delay the expansion of child
    // nodes for performance reasons.
    this.delayExpansion =
        this.volumeInfo.source === VolumeManagerCommon.Source.NETWORK &&
        (this.volumeInfo.volumeType ===
             VolumeManagerCommon.VolumeType.PROVIDED ||
         this.volumeInfo.volumeType === VolumeManagerCommon.VolumeType.SMB);

    // Set helper attribute for testing.
    if (window.IN_TEST) {
      this.setAttribute('volume-type-for-testing', this.volumeInfo_.volumeType);
      this.setAttribute('dir-type', 'VolumeItem');
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      this.setAttribute('drive-label', this.volumeInfo_.driveLabel);
    }

    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Element'.
    this.setupIcon_(this.querySelector('.icon'), this.volumeInfo_);
    if (isOneDrive(modelItem.volumeInfo)) {
      this.toggleAttribute('one-drive', true);
    }

    // Attach a placeholder for rename input text box and the eject icon if the
    // volume is ejectable
    if ((modelItem.volumeInfo_.source === VolumeManagerCommon.Source.DEVICE &&
         modelItem.volumeInfo_.volumeType !==
             VolumeManagerCommon.VolumeType.MTP) ||
        modelItem.volumeInfo_.source === VolumeManagerCommon.Source.FILE) {
      // This placeholder is added to allow to put textbox before eject button
      // while executing renaming action on external drive.
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'VolumeItem'.
      this.setupRenamePlaceholder_(this.rowElement);
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'VolumeItem'.
      this.setupEjectButton_(this.rowElement, modelItem.label);
    }

    // Sets up context menu of the item.
    if (tree.contextMenuForRootItems) {
      // @ts-ignore: error TS2341: Property 'setContextMenu_' is private and
      // only accessible within class 'DirectoryItem'.
      this.setContextMenu_(tree.contextMenuForRootItems);
    }

    /**
     * Whether the display root has been resolved.
     * @private @type {boolean}
     */
    this.resolved_ = false;

    // Populate children of this volume using resolved display root. For SMB
    // shares, avoid prefetching sub directories to delay authentication.
    if (modelItem.volumeInfo_.providerId !== '@smb' &&
        modelItem.volumeInfo_.volumeType !==
            VolumeManagerCommon.VolumeType.SMB) {
      this.volumeInfo_.resolveDisplayRoot(
          // @ts-ignore: error TS6133: 'displayRoot' is declared but its value
          // is never read.
          (displayRoot) => {
            this.resolved_ = true;
            this.updateSubDirectories(false /* recursive */);
          },
          (error) => {
            console.warn(
                'Failed to resolve display root of',
                modelItem.volumeInfo_.volumeType, 'due to', error);
          });
    }
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'opt_errorCallback' implicitly has an
  // 'any' type.
  updateSubDirectories(recursive, opt_successCallback, opt_errorCallback) {
    if (!this.resolved_) {
      return;
    }

    if (this.volumeInfo.volumeType ===
        VolumeManagerCommon.VolumeType.MEDIA_VIEW) {
      // If this is a media-view volume, we don't show child directories.
      // (Instead, we provide flattened files in the file list.)
      opt_successCallback && opt_successCallback();
    } else {
      DirectoryItem.prototype.updateSubDirectories.call(
          this, recursive, opt_successCallback, opt_errorCallback);
    }
  }

  /**
   * Change current entry to this volume's root directory.
   * @override
   */
  activate() {
    const directoryModel = this.parentTree_.directoryModel;
    // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any' type.
    const onEntryResolved = (entry) => {
      this.resolved_ = true;
      // Changes directory to the model item's root directory if needed.
      if (!isSameEntry(directoryModel.getCurrentDirEntry(), entry)) {
        directoryModel.changeDirectoryEntry(entry);
      }
      // In case of failure in resolveDisplayRoot() in the volume's constructor,
      // update the volume's children here.
      this.updateSubDirectories(false);
    };

    this.volumeInfo_.resolveDisplayRoot(onEntryResolved, () => {
      // Error, the display root is not available. It may happen on Drive.
      this.parentTree_.dataModel.onItemNotFoundError(this.modelItem);
    });
  }

  /**
   * Set up icon of this volume item.
   * @param {Element} icon Icon element to be setup.
   * @param {import('../../../externs/volume_info.js').VolumeInfo} volumeInfo
   *     VolumeInfo determines the icon type.
   * @private
   */
  setupIcon_(icon, volumeInfo) {
    icon.classList.add('item-icon');

    const backgroundImage =
        iconSetToCSSBackgroundImageValue(volumeInfo.iconSet);
    if (backgroundImage !== 'none') {
      icon.setAttribute('style', 'background-image: ' + backgroundImage);
    } else if (VolumeManagerCommon.shouldProvideIcons(
                   assert(volumeInfo.volumeType))) {
      icon.setAttribute('use-generic-provided-icon', '');
    }

    if (volumeInfo.volumeType == VolumeManagerCommon.VolumeType.GUEST_OS) {
      icon.setAttribute(
          'volume-type-icon', vmTypeToIconName(volumeInfo.vmType));
    } else {
      icon.setAttribute(
          'volume-type-icon', /** @type {string} */ (volumeInfo.volumeType));
    }

    if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.MEDIA_VIEW) {
      const subtype = VolumeManagerCommon.getMediaViewRootTypeFromVolumeId(
          volumeInfo.volumeId);
      icon.setAttribute('volume-subtype', subtype);
    } else {
      icon.setAttribute(
          'volume-subtype',
          /** @type {string} */ (volumeInfo.deviceType) || '');
    }
  }

  /**
   * Set up rename input textbox placeholder element. Place it just after the
   * tree row '.label' class element.
   * @param {!HTMLElement} rowElement Tree row element.
   * @private
   */
  setupRenamePlaceholder_(rowElement) {
    const placeholder = document.createElement('span');
    placeholder.className = 'rename-placeholder';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    rowElement.querySelector('.label').insertAdjacentElement(
        'afterend', placeholder);
  }

  /**
   * Directory entry for the display root, whose initial value is null.
   * @type {DirectoryEntry}
   * @override
   */
  get entry() {
    return this.volumeInfo_.displayRoot;
  }

  /**
   * @type {!import('../../../externs/volume_info.js').VolumeInfo}
   */
  get volumeInfo() {
    return this.volumeInfo_;
  }

  /**
   * @type {!NavigationModelVolumeItem}
   */
  get modelItem() {
    return this.modelItem_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DriveVolumeItem

/**
 * A TreeItem which represents a Drive volume. Drive volume has fake entries
 * such as Shared Drives, Shared with me, and Offline in it.
 */
export class DriveVolumeItem extends VolumeItem {
  /**
   * @param {!NavigationModelVolumeItem} modelItem NavigationModelItem of this
   *     volume.
   * @param {!DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(modelItem, tree) {
    super(modelItem, tree);
    this.__proto__ = DriveVolumeItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'DriveVolumeItem');
    }

    this.classList.add('drive-volume');
  }

  /**
   * Invoked when the tree item is clicked.
   *
   * @param {Event} e Click event.
   * @override
   */
  handleClick(e) {
    super.handleClick(e);

    // @ts-ignore: error TS2345: Argument of type 'EventTarget | null' is not
    // assignable to parameter of type 'EventTarget'.
    this.selectDisplayRoot_(e.target);

    DirectoryItemTreeBaseMethods.recordUMASelectedEntry.call(
        this, e, VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT, true);
  }

  /**
   * Creates Shared Drives root if there is any team drive, if there is no team
   * drive, then it removes the root.
   *
   * Since we don't currently support any functionality with just the grand root
   * (e.g. you can't create a new team drive from the root yet), remove/don't
   * create the grand root so it can't be reached via keyboard.
   * If there is at least one Shared Drive, add/show the Shared Drives grand
   * root.
   *
   * @return {!Promise<SubDirectoryItem>} Resolved with Shared Drive Grand Root
   * SubDirectoryItem instance, or undefined when it shouldn't exist.
   * @private
   */
  createSharedDrivesGrandRoot_() {
    return new Promise((resolve) => {
      const sharedDriveGrandRoot = this.volumeInfo_.sharedDriveDisplayRoot;
      if (!sharedDriveGrandRoot) {
        // Shared Drive is disabled.
        // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
        // Promise()' needs a JSDoc hint to produce a 'resolve' that can be
        // called without arguments.
        resolve();
        return;
      }

      // @ts-ignore: error TS7034: Variable 'index' implicitly has type 'any' in
      // some locations where its type cannot be determined.
      let index;
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DriveVolumeItem'.
      for (let i = 0; i < this.items.length; i++) {
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DriveVolumeItem'.
        const entry = this.items[i] && this.items[i].entry;
        if (entry && isSameEntry(entry, sharedDriveGrandRoot)) {
          index = i;
          break;
        }
      }

      const reader = sharedDriveGrandRoot.createReader();
      reader.readEntries((results) => {
        recordSmallCount('TeamDrivesCount', results.length);
        // Only create grand root if there is at least 1 child/result.
        if (results.length) {
          // @ts-ignore: error TS7005: Variable 'index' implicitly has an 'any'
          // type.
          if (index !== undefined) {
            // @ts-ignore: error TS7005: Variable 'index' implicitly has an
            // 'any' type.
            this.items[index].hidden = false;
            // @ts-ignore: error TS7005: Variable 'index' implicitly has an
            // 'any' type.
            resolve(this.items[index]);
            return;
          }

          // Create if it doesn't exist yet.
          const label = getEntryLabel(
                            this.parentTree_.volumeManager_.getLocationInfo(
                                sharedDriveGrandRoot),
                            sharedDriveGrandRoot) ||
              '';
          const item = new SubDirectoryItem(
              label, sharedDriveGrandRoot, this, this.parentTree_);
          // @ts-ignore: error TS2339: Property 'addAt' does not exist on type
          // 'DriveVolumeItem'.
          this.addAt(item, 1);
          item.updateExpandIcon();
          resolve(item);
          return;
        } else {
          // When there is no team drive, the grand root should be removed.
          // @ts-ignore: error TS2339: Property 'items' does not exist on type
          // 'DriveVolumeItem'.
          if (index && this.items[index].parentItem) {
            // @ts-ignore: error TS2339: Property 'items' does not exist on type
            // 'DriveVolumeItem'.
            this.items[index].parentItem.remove(this.items[index]);
          }
          // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
          // Promise()' needs a JSDoc hint to produce a 'resolve' that can be
          // called without arguments.
          resolve();
          return;
        }
      });
    });
  }

  /**
   * Creates Computers root if there is any computer. If there is no computer,
   * then it removes the root.
   *
   * Since we don't currently support any functionality with just the grand root
   * (e.g. you can't create a new computer from the root yet), remove/don't
   * create the grand root so it can't be reached via keyboard.
   * If there is at least one Computer, add/show the Computer grand root.
   *
   * @return {!Promise<SubDirectoryItem>} Resolved with Computer Grand Root
   * SubDirectoryItem instance, or undefined when it shouldn't exist.
   * @private
   */
  createComputersGrandRoot_() {
    return new Promise((resolve) => {
      const computerGrandRoot = this.volumeInfo_.computersDisplayRoot;
      if (!computerGrandRoot) {
        // Computer is disabled.
        // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
        // Promise()' needs a JSDoc hint to produce a 'resolve' that can be
        // called without arguments.
        resolve();
        return;
      }

      // @ts-ignore: error TS7034: Variable 'index' implicitly has type 'any' in
      // some locations where its type cannot be determined.
      let index;
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DriveVolumeItem'.
      for (let i = 0; i < this.items.length; i++) {
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DriveVolumeItem'.
        const entry = this.items[i] && this.items[i].entry;
        if (entry && isSameEntry(entry, computerGrandRoot)) {
          index = i;
          break;
        }
      }

      const reader = computerGrandRoot.createReader();
      reader.readEntries((results) => {
        recordSmallCount('ComputersCount', results.length);
        // Only create grand root if there is at least 1 child/result.
        if (results.length) {
          // @ts-ignore: error TS7005: Variable 'index' implicitly has an 'any'
          // type.
          if (index !== undefined) {
            // @ts-ignore: error TS7005: Variable 'index' implicitly has an
            // 'any' type.
            this.items[index].hidden = false;
            // @ts-ignore: error TS7005: Variable 'index' implicitly has an
            // 'any' type.
            resolve(this.items[index]);
            return;
          }

          // Create if it doesn't exist yet.
          const label = getEntryLabel(
                            // @ts-ignore: error TS2345: Argument of type
                            // 'EntryLocation | null' is not assignable to
                            // parameter of type 'EntryLocation'.
                            this.parentTree_.volumeManager_.getLocationInfo(
                                computerGrandRoot),
                            computerGrandRoot) ||
              '';
          const item = new SubDirectoryItem(
              label, computerGrandRoot, this, this.parentTree_);
          // We want to show "Computers" after "Shared Drives", the
          // computersIndexPosition_() helper function will work out the correct
          // index to place "Computers" at.
          const position = this.computersIndexPosition_();
          // @ts-ignore: error TS2339: Property 'addAt' does not exist on type
          // 'DriveVolumeItem'.
          this.addAt(item, position);
          item.updateExpandIcon();
          resolve(item);
          return;
        } else {
          // When there is no computer, the grand root should be removed.
          // @ts-ignore: error TS2339: Property 'items' does not exist on type
          // 'DriveVolumeItem'.
          if (index && this.items[index].parentItem) {
            // @ts-ignore: error TS2339: Property 'items' does not exist on type
            // 'DriveVolumeItem'.
            this.items[index].parentItem.remove(this.items[index]);
          }
          // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
          // Promise()' needs a JSDoc hint to produce a 'resolve' that can be
          // called without arguments.
          resolve();
          return;
        }
      });
    });
  }

  /**
   * Change current entry to the entry corresponding to My Drive.
   * @override
   */
  activate() {
    super.activate();
    this.selectDisplayRoot_(this);
  }

  /**
   * Select Drive's display root.
   * @param {EventTarget} target The event target.
   */
  selectDisplayRoot_(target) {
    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'EventTarget'.
    if (!target.classList.contains('expand-icon')) {
      // If the Drive volume is clicked, select one of the children instead of
      // this item itself.
      this.volumeInfo_.resolveDisplayRoot(
          (displayRoot) => {
            this.searchAndSelectByEntry(displayRoot);
          },
          (error) => {
            console.warn(
                'Unable to select display root for', target, 'due to', error);
          });
    }
  }

  /**
   * Retrieves the latest subdirectories and update them on the tree.
   * @param {boolean} recursive True if the update is recursively.
   * @override
   */
  // @ts-ignore: error TS6133: 'recursive' is declared but its value is never
  // read.
  updateSubDirectories(recursive) {
    if (!this.entry || this.hasChildren || this.disabled) {
      return;
    }

    let entries = [this.entry];

    const teamDrivesDisplayRoot = this.volumeInfo_.sharedDriveDisplayRoot;
    if (teamDrivesDisplayRoot) {
      entries.push(teamDrivesDisplayRoot);
    }

    const computersDisplayRoot = this.volumeInfo_.computersDisplayRoot;
    if (computersDisplayRoot) {
      entries.push(computersDisplayRoot);
    }

    // Drive volume has children including fake entries (offline, recent, ...)
    const fakeEntries = [];
    // @ts-ignore: error TS2341: Property 'fakeEntriesVisible_' is private and
    // only accessible within class 'DirectoryTree'.
    if (this.parentTree_.fakeEntriesVisible_) {
      for (const key in this.volumeInfo_.fakeEntries) {
        fakeEntries.push(this.volumeInfo_.fakeEntries[key]);
      }
      // This list is sorted by URL on purpose.
      fakeEntries.sort((a, b) => {
        // @ts-ignore: error TS18048: 'b' is possibly 'undefined'.
        if (a.toURL() === b.toURL()) {
          return 0;
        }
        // @ts-ignore: error TS18048: 'a' is possibly 'undefined'.
        return b.toURL() > a.toURL() ? 1 : -1;
      });
      // @ts-ignore: error TS2769: No overload matches this call.
      entries = entries.concat(fakeEntries);
    }

    for (let i = 0; i < entries.length; i++) {
      // Only create the team drives root if there is at least 1 team drive.
      const entry = entries[i];
      if (entry === teamDrivesDisplayRoot) {
        this.createSharedDrivesGrandRoot_();
      } else if (entry === computersDisplayRoot) {
        this.createComputersGrandRoot_();
      } else {
        const label =
            getEntryLabel(
                // @ts-ignore: error TS2345: Argument of type
                // 'FileSystemDirectoryEntry | undefined' is not assignable to
                // parameter of type 'FileSystemEntry | FilesAppEntry'.
                this.parentTree_.volumeManager_.getLocationInfo(entry),
                // @ts-ignore: error TS2345: Argument of type
                // 'FileSystemDirectoryEntry | undefined' is not assignable to
                // parameter of type 'FileSystemEntry | FilesAppEntry'.
                entry) ||
            '';
        // @ts-ignore: error TS2345: Argument of type 'FileSystemDirectoryEntry
        // | undefined' is not assignable to parameter of type
        // 'FileSystemDirectoryEntry'.
        const item = new SubDirectoryItem(label, entry, this, this.parentTree_);
        // @ts-ignore: error TS2339: Property 'add' does not exist on type
        // 'DriveVolumeItem'.
        this.add(item);
        item.updateSubDirectories(false);
      }
    }
  }

  /**
   * Searches for the changed directory in the current subtree, and if it is
   * found then updates it.
   *
   * @param {!DirectoryEntry} changedDirectoryEntry The entry of the changed
   *     directory.
   * @override
   */
  updateItemByEntry(changedDirectoryEntry) {
    const isTeamDriveChild = isSharedDriveEntry(changedDirectoryEntry);

    // If Shared Drive grand root has been removed and we receive an update for
    // an team drive, we need to create the Shared Drive grand root.
    if (isTeamDriveChild) {
      this.createSharedDrivesGrandRoot_().then((sharedDriveGrandRootItem) => {
        if (sharedDriveGrandRootItem) {
          sharedDriveGrandRootItem.updateItemByEntry(changedDirectoryEntry);
        }
      });
      return;
    }

    const isComputersChild = isComputersEntry(changedDirectoryEntry);
    // If Computers grand root has been removed and we receive an update for an
    // computer, we need to create the Computers grand root.
    if (isComputersChild) {
      this.createComputersGrandRoot_().then((computersGrandRootItem) => {
        if (computersGrandRootItem) {
          computersGrandRootItem.updateItemByEntry(changedDirectoryEntry);
        }
      });
      return;
    }

    // NOTE: It's possible that the DriveVolumeItem hasn't populated its
    // children yet.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DriveVolumeItem'.
    if (this.items[0]) {
      // Must be under "My Drive", which is always the first item.
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DriveVolumeItem'.
      this.items[0].updateItemByEntry(changedDirectoryEntry);
    }
  }

  /**
   * Select the item corresponding to the given entry.
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The directory entry to be
   *     selected. Can be a fake.
   * @return {!Promise<void>}
   * @override
   */
  async selectByEntry(entry) {
    // Find the item to be selected among children.
    await this.searchAndSelectByEntry(entry);
  }

  /**
   * Return the index where we want to display the "Computers" root.
   * @private
   */
  computersIndexPosition_() {
    // We want the order to be
    // - My Drive
    // - Shared Drives (if the user has any)
    // - Computers (if the user has any)
    // So if the user has team drives we want index position 2, otherwise index
    // position 1.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DriveVolumeItem'.
    for (let i = 0; i < this.items.length; i++) {
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DriveVolumeItem'.
      const item = this.items[i];
      if (!item.entry) {
        continue;
      }
      if (isSharedDriveEntry(item.entry)) {
        return 2;
      }
    }
    return 1;
  }

  // Overrides the property 'expanded' to prevent Drive volume from shrinking.
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'VolumeItem'.
  get expanded() {
    // @ts-ignore: error TS2684: The 'this' context of type '(() => any) |
    // undefined' is not assignable to method's 'this' of type '(this: this) =>
    // any'.
    return Object.getOwnPropertyDescriptor(TreeItem.prototype, 'expanded')
        .get.call(this);
  }
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'VolumeItem'.
  set expanded(b) {
    // @ts-ignore: error TS2684: The 'this' context of type '((v: any) => void)
    // | undefined' is not assignable to method's 'this' of type '(this: this,
    // args_0: any) => void'.
    Object.getOwnPropertyDescriptor(TreeItem.prototype, 'expanded')
        .set.call(this, b);
    // When Google Drive is expanded while it is selected, select the My Drive.
    if (b) {
      if (this.selected && this.entry) {
        this.selectByEntry(this.entry);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShortcutItem

/**
 * A TreeItem which represents a shortcut for Drive folder.
 * Shortcut items are displayed as top-level children of DirectoryTree.
 */
export class ShortcutItem extends FilesTreeItem {
  /**
   * @param {!NavigationModelShortcutItem} modelItem NavigationModelItem of this
   *     volume.
   * @param {!DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(modelItem, tree) {
    super(modelItem.entry.name, tree);
    this.__proto__ = ShortcutItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'ShortcutItem');
    }

    this.dirEntry_ = modelItem.entry;
    this.modelItem_ = modelItem;
    this.disabled = modelItem.disabled;

    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.add('item-icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.setAttribute('volume-type-icon', 'shortcut');

    if (tree.contextMenuForRootItems) {
      this.setContextMenu_(tree.contextMenuForRootItems);
    }
  }

  /**
   * Finds a parent directory of the {@code entry} in {@code this}, and
   * invokes the DirectoryItem.selectByEntry() of the found directory.
   *
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The entry to be searched
   *     for. Can be a fake.
   * @return {boolean} True if the parent item is found.
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  searchAndSelectByEntry(entry) {
    // Always false as shortcuts have no children.
    return false;
  }

  /**
   * Invoked when the tree item is clicked.
   *
   * @param {Event} e Click event.
   * @override
   */
  handleClick(e) {
    super.handleClick(e);

    // Do not activate with right click.
    // @ts-ignore: error TS2339: Property 'button' does not exist on type
    // 'Event'.
    if (e.button === 2) {
      return;
    }

    // Resets file selection when a volume is clicked.
    this.parentTree_.directoryModel.clearSelection();

    // @ts-ignore: error TS2339: Property 'tree' does not exist on type
    // 'ShortcutItem'.
    const location = this.tree.volumeManager.getLocationInfo(this.entry);
    DirectoryItemTreeBaseMethods.recordUMASelectedEntry.call(
        this, e, location.rootType, location.isRootEntry);
  }

  /**
   * Select the item corresponding to the given entry.
   * @param {!DirectoryEntry} entry The directory entry to be selected.
   */
  selectByEntry(entry) {
    if (isSameEntry(entry, this.entry)) {
      this.selected = true;
    }
  }

  /**
   * Sets the context menu for shortcut items.
   * @param {!Menu} menu Menu to be set.
   * @private
   */
  setContextMenu_(menu) {
    contextMenuHandler.setContextMenu(this, menu);
  }

  /**
   * Change current entry to the entry corresponding to this shortcut.
   * @override
   */
  activate() {
    const directoryModel = this.parentTree_.directoryModel;
    // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any' type.
    const onEntryResolved = (entry) => {
      // Changes directory to the model item's root directory if needed.
      if (!isSameEntry(directoryModel.getCurrentDirEntry(), entry)) {
        recordUserAction('FolderShortcut.Navigate');
        directoryModel.changeDirectoryEntry(entry);
      }
    };

    // For shortcuts we already have an Entry, but it has to be resolved again
    // in case, it points to a non-existing directory.
    window.webkitResolveLocalFileSystemURL(
        this.entry.toURL(), onEntryResolved, () => {
          // Error, the entry can't be re-resolved. It may happen for shortcuts
          // which targets got removed after resolving the Entry during
          // initialization.
          this.parentTree_.dataModel.onItemNotFoundError(this.modelItem);
        });
  }

  /**
   * The DirectoryEntry corresponding to this DirectoryItem.
   */
  get entry() {
    return this.dirEntry_;
  }

  /**
   * @type {!NavigationModelVolumeItem}
   */
  get modelItem() {
    // @ts-ignore: error TS2739: Type 'NavigationModelShortcutItem' is missing
    // the following properties from type 'NavigationModelVolumeItem':
    // volumeInfo_, volumeInfo
    return this.modelItem_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AndroidAppItem

/**
 * A TreeItem representing an Android picker app. These Android app items are
 * shown as top-level volume entries of the DirectoryTree.
 */
class AndroidAppItem extends FilesTreeItem {
  /**
   * @param {!NavigationModelAndroidAppItem} modelItem NavigationModelItem
   *     associated with this volume.
   * @param {!DirectoryTree} tree Directory tree.
   */
  constructor(modelItem, tree) {
    super(modelItem.androidApp.name, tree);
    this.__proto__ = AndroidAppItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'AndroidAppItem');
    }

    this.modelItem_ = modelItem;
    this.disabled = modelItem.disabled;

    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.add('item-icon');

    if (modelItem.androidApp.iconSet) {
      const backgroundImage =
          iconSetToCSSBackgroundImageValue(modelItem.androidApp.iconSet);
      if (backgroundImage !== 'none') {
        // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
        icon.setAttribute('style', 'background-image: ' + backgroundImage);
      }
    }

    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.setAttribute('use-generic-provided-icon', '');

    // Use aria-describedby attribute to let ChromeVox users know that the link
    // launches an external app window.
    this.setAttribute('aria-describedby', 'external-link-label');

    // Create an external link icon.
    const externalLinkIcon = document.createElement('span');
    externalLinkIcon.className = 'external-link-icon align-right-icon';

    // Append external-link iron-icon.
    const ironIcon = document.createElement('iron-icon');
    ironIcon.setAttribute('icon', `files20:external-link`);
    externalLinkIcon.appendChild(ironIcon);

    // Add the external-link as the last element of the tree row content.
    // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
    // 'AndroidAppItem'.
    const label = this.rowElement.querySelector('.label');
    label.parentElement.appendChild(externalLinkIcon);

    // Ensure the link icon shows when the directory tree is too narrow.
    label.setAttribute('style', 'margin-inline-end: 2px; min-width: 0;');
  }

  /**
   * Invoked when the tree item is clicked.
   *
   * @param {Event} e Click event.
   * @override
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleClick(e) {
    chrome.fileManagerPrivate.selectAndroidPickerApp(
        this.modelItem_.androidApp, () => {
          if (chrome.runtime.lastError) {
            console.error(
                'selectAndroidPickerApp error: ',
                chrome.runtime.lastError.message);
          } else {
            window.close();
          }
        });
  }
}

////////////////////////////////////////////////////////////////////////////////
// FakeItem

/**
 * FakeItem is used by Recent files, Drive, Crostini and other Guest OSs.
 */
export class FakeItem extends FilesTreeItem {
  /**
   * @param {!VolumeManagerCommon.RootType} rootType root type.
   * @param {!NavigationModelFakeItem} modelItem
   * @param {!DirectoryTree} tree Current tree, which contains this item.
   */
  constructor(rootType, modelItem, tree) {
    super(modelItem.label, tree);
    this.__proto__ = FakeItem.prototype;

    if (window.IN_TEST) {
      this.setAttribute('dir-type', 'FakeItem');
    }

    this.directoryModel_ = tree.directoryModel;
    this.dirEntry_ = modelItem.entry;
    this.modelItem_ = modelItem;
    this.rootType_ = rootType;
    this.disabled = modelItem.disabled;

    const icon = this.querySelector('.icon');
    // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
    icon.classList.add('item-icon');
    // @ts-ignore: error TS2339: Property 'iconName' does not exist on type
    // 'FilesAppEntry'.
    if (this.entry && this.entry.iconName) {
      // @ts-ignore: error TS2339: Property 'iconName' does not exist on type
      // 'FilesAppEntry'.
      icon.setAttribute('root-type-icon', this.entry.iconName);
    } else {
      // @ts-ignore: error TS18047: 'icon' is possibly 'null'.
      icon.setAttribute('root-type-icon', rootType);
    }

    if (isRecentRootType(rootType)) {
      // @ts-ignore: error TS2339: Property 'fileCategory' does not exist on
      // type 'FilesAppEntry'.
      if (this.dirEntry_.fileCategory) {
        // @ts-ignore: error TS2339: Property 'fileType' does not exist on type
        // 'FilesAppEntry'.
        icon.setAttribute('recent-file-type', this.dirEntry_.fileType);
      } else {  // Recent tab scroll fix: crbug.com/1027973.
        // @ts-ignore: error TS2339: Property 'scrollIntoViewIfNeeded' does not
        // exist on type 'HTMLElement'.
        this.labelElement.scrollIntoViewIfNeeded = () => {
          this.scrollIntoView(true);
        };
      }
    }

    if (tree.disabledContextMenu) {
      contextMenuHandler.setContextMenu(this, tree.disabledContextMenu);
    }
  }

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry
   * @return {boolean} True if the parent item is found.
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  searchAndSelectByEntry(entry) {
    return false;
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleClick(e) {
    super.handleClick(e);

    DirectoryItemTreeBaseMethods.recordUMASelectedEntry.call(
        this, e, this.rootType_, true);
  }

  /**
   * @param {!DirectoryEntry} entry
   */
  selectByEntry(entry) {
    if (isSameEntry(entry, this.entry)) {
      this.selected = true;
    }
  }

  /**
   * Executes the command.
   * @override
   */
  activate() {
    // @ts-ignore: error TS2345: Argument of type 'FilesAppEntry' is not
    // assignable to parameter of type 'FileSystemDirectoryEntry |
    // FilesAppDirEntry'.
    this.parentTree_.directoryModel.activateDirectoryEntry(this.entry);
  }

  /**
   * FakeItem doesn't really have sub-directories, it's defined here only to
   * have the same API of other Items on this file.
   */
  // @ts-ignore: error TS7006: Parameter 'opt_errorCallback' implicitly has an
  // 'any' type.
  updateSubDirectories(recursive, opt_successCallback, opt_errorCallback) {
    return opt_successCallback && opt_successCallback();
  }

  /**
   * FakeItem's do not have shared status/icon.
   */
  updateDriveSpecificIcons() {}

  /**
   * The DirectoryEntry corresponding to this DirectoryItem.
   */
  get entry() {
    return this.dirEntry_;
  }

  /**
   * @type {!NavigationModelVolumeItem}
   */
  get modelItem() {
    // @ts-ignore: error TS2322: Type 'NavigationModelFakeItem' is not
    // assignable to type 'NavigationModelVolumeItem'.
    return this.modelItem_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DirectoryTree

/**
 * Tree of directories on the middle bar. This element is also the root of
 * items, in other words, this is the parent of the top-level items.
 */
export class DirectoryTree extends Tree {
  constructor() {
    super();

    /** @type {?DirectoryItem} */
    this.activeItem_ = null;

    /** @type {?DirectoryItem} */
    this.lastActiveItem_ = null;

    /** @type {NavigationListModel} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'NavigationListModel'.
    this.dataModel_ = null;

    /** @type {number} */
    this.sequence_ = 0;

    /** @type {DirectoryModel} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'DirectoryModel'.
    this.directoryModel_ = null;

    /** @type {VolumeManager} this is set in decorate() */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'VolumeManager'.
    this.volumeManager_ = null;

    /** @type {MetadataModel} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'MetadataModel'.
    this.metadataModel_ = null;

    /** @type {FileFilter} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'FileFilter'.
    this.fileFilter_ = null;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @type {?function(*)} */
    this.onListContentChangedBound_ = null;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @type {?function(!chrome.fileManagerPrivate.FileWatchEvent)} */
    this.privateOnDirectoryChangedBound_ = null;
  }

  get typeName() {
    return 'directory_tree';
  }

  /**
   * Decorates an element.
   * @param {!DirectoryModel} directoryModel Current DirectoryModel.
   * @param {!VolumeManager} volumeManager VolumeManager of the system.
   * @param {!MetadataModel} metadataModel Shared MetadataModel instance.
   * @param {!FileOperationManager} fileOperationManager
   * @param {boolean} fakeEntriesVisible True if it should show the fakeEntries.
   */
  decorateDirectoryTree(
      // @ts-ignore: error TS6133: 'fileOperationManager' is declared but its
      // value is never read.
      directoryModel, volumeManager, metadataModel, fileOperationManager,
      fakeEntriesVisible) {
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Tree'.
    Tree.prototype.decorate.call(this);

    this.sequence_ = 0;
    this.directoryModel_ = directoryModel;
    this.volumeManager_ = volumeManager;
    this.metadataModel_ = metadataModel;

    this.fileFilter_ = this.directoryModel_.getFileFilter();
    this.fileFilter_.addEventListener(
        'changed', this.onFilterChanged_.bind(this));

    this.directoryModel_.addEventListener(
        'directory-changed', this.onCurrentDirectoryChanged_.bind(this));

    this.addEventListener(
        'scroll', this.onTreeScrollEvent_.bind(this), {passive: true});

    // @ts-ignore: error TS6133: 'event' is declared but its value is never
    // read.
    this.addEventListener('click', (event) => {
      // Chromevox triggers |click| without switching focus, we force the focus
      // here so we can handle further keyboard/mouse events to expand/collapse
      // directories.
      if (document.activeElement === document.body) {
        this.focus();
      }
    });

    this.addEventListener(
        'mouseover', this.onMouseOver_.bind(this), {passive: true});

    this.privateOnDirectoryChangedBound_ =
        this.onDirectoryContentChanged_.bind(this);
    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        this.privateOnDirectoryChangedBound_);

    /**
     * Flag to show fake entries in the tree.
     * @type {boolean}
     * @private
     */
    this.fakeEntriesVisible_ = fakeEntriesVisible;

    // For Search V2 subscribe to the store so that we can listen to search
    // becoming active and inactive. We use this to hide or show the highlight
    // of the active item in the directory tree.
    /** @type {!SearchData|undefined} */
    // @ts-ignore: error TS2739: Type '{}' is missing the following properties
    // from type 'SearchData': status, query, options
    this.cachedSearchState_ = {};

    /**
     * Subscribe to the store so that we can listen to ODFS getting enabled or
     * disabled. When ODFS first gets added in the store,
     * `isODFSVolumeDisabled_` gets initialized to the right value and the
     * directory tree and its data model are updated through other mechanisms.
     * @type {boolean}
     */
    this.isODFSVolumeDisabled_ = false;
    getStore().subscribe(this);
  }

  /**
   * @param {!State} state
   */
  onStateChanged(state) {
    // Search.
    const searchState = state.search;
    if (searchState !== this.cachedSearchState_) {
      this.cachedSearchState_ = searchState;
      if (searchState === undefined) {
        this.setActiveItemHighlighted_(true);
      } else {
        if (searchState.status === undefined) {
          this.setActiveItemHighlighted_(true);
        } else if (
            searchState.status === PropStatus.STARTED && searchState.query) {
          this.setActiveItemHighlighted_(
              (searchState.options || {}).location ===
              SearchLocation.THIS_FOLDER);
        }
      }
    }

    // ODFS.
    const odfsDisabledUpdated =
        Object.values(state.volumes)
            .some(
                volume => volume && isOneDriveId(volume.providerId) &&
                    !!volume.isDisabled !== this.isODFSVolumeDisabled_);
    if (odfsDisabledUpdated) {
      this.isODFSVolumeDisabled_ = !this.isODFSVolumeDisabled_;
      // Refresh data model.
      this.dataModel.refreshNavigationItems();
      // Remove ODFS volumes from the directoryTree so that they get redrawn
      // with the right attributes.
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryTree'.
      for (let i = 0; i < this.items.length; ++i) {
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DirectoryTree'.
        const treeItem = this.items[i];
        if (isOneDrive(treeItem.modelItem.volumeInfo)) {
          // @ts-ignore: error TS2554: Expected 0 arguments, but got 1.
          this.remove(treeItem);
          // Decrement to account for the removed item.
          --i;
        }
      }
      // Force-redraw directory tree.
      this.redraw(true);
    }
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onMouseOver_(event) {
    this.maybeShowToolTip(event);
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  maybeShowToolTip(event) {
    const target = event.composedPath()[0];
    if (!target) {
      return;
    }
    if (!(target.classList.contains('tree-row') &&
          target.parentElement?.label)) {
      return;
    }
    const labelElement = target.querySelector('.label');
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, target.parentElement.label);
  }

  /**
   * Updates and selects new directory.
   * @param {!DirectoryEntry} parentDirectory Parent directory of new directory.
   * @param {!DirectoryEntry} newDirectory
   */
  updateAndSelectNewDirectory(parentDirectory, newDirectory) {
    // Expand parent directory.
    const parentItem =
        DirectoryItemTreeBaseMethods.getItemByEntry.call(this, parentDirectory);
    parentItem.expanded = true;

    // If new directory is already added to the tree, just select it.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem'.
    for (let i = 0; i < parentItem.items.length; i++) {
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryItem'.
      const item = parentItem.items[i];
      if (isSameEntry(item.entry, newDirectory)) {
        this.selectedItem = item;
        return;
      }
    }

    // Create new item, and add it.
    const newDirectoryItem =
        new SubDirectoryItem(newDirectory.name, newDirectory, parentItem, this);

    let addAt = 0;
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryItem'.
    while (addAt < parentItem.items.length &&
           // @ts-ignore: error TS2339: Property 'items' does not exist on type
           // 'DirectoryItem'.
           compareName(parentItem.items[addAt].entry, newDirectory) < 0) {
      addAt++;
    }

    // @ts-ignore: error TS2339: Property 'addAt' does not exist on type
    // 'DirectoryItem'.
    parentItem.addAt(newDirectoryItem, addAt);
    this.selectedItem = newDirectoryItem;
  }

  /**
   * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
   *
   * @param {boolean} recursive True if the all visible sub-directories are
   *     updated recursively including left arrows. If false, the update walks
   *     only immediate child directories without arrows.
   */
  updateSubElementsFromList(recursive) {
    // First, current items which is not included in the dataModel should be
    // removed.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryTree'.
    for (let i = 0; i < this.items.length;) {
      let found = false;
      for (let j = 0; j < this.dataModel.length; j++) {
        // Comparison by references, which is safe here, as model items are long
        // living.
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DirectoryTree'.
        if (this.items[i].modelItem === this.dataModel.item(j)) {
          found = true;
          break;
        }
      }
      if (!found) {
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DirectoryTree'.
        if (this.items[i].selected) {
          // @ts-ignore: error TS2339: Property 'items' does not exist on type
          // 'DirectoryTree'.
          this.items[i].selected = false;
        }
        // @ts-ignore: error TS2339: Property 'items' does not exist on type
        // 'DirectoryTree'.
        this.remove(this.items[i]);
      } else {
        i++;
      }
    }

    // Next, insert items which is in dataModel but not in current items.
    let modelIndex = 0;
    let itemIndex = 0;
    // Initialize with first item's section so the first root doesn't get a
    // divider line at the top.
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    let previousSection = this.dataModel.item(modelIndex).section;
    while (modelIndex < this.dataModel.length) {
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryTree'.
      const currentItem = this.items[itemIndex];
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryTree'.
      if (itemIndex < this.items.length &&
          currentItem.modelItem === this.dataModel.item(modelIndex)) {
        const modelItem = currentItem.modelItem;
        if (previousSection !== modelItem.section) {
          currentItem.setAttribute('section-start', modelItem.section);
          previousSection = modelItem.section;
        }
        if (recursive && currentItem instanceof VolumeItem) {
          currentItem.updateSubDirectories(true);
        }
        // EntryListItem can contain volumes that might have been updated: ask
        // them to re-draw.
        if (currentItem instanceof EntryListItem) {
          currentItem.updateSubDirectories(recursive);
        }
      } else {
        const modelItem = this.dataModel.item(modelIndex);
        if (modelItem) {
          const item = DirectoryTree.createDirectoryItem(modelItem, this);
          if (item) {
            // @ts-ignore: error TS2339: Property 'addAt' does not exist on type
            // 'DirectoryTree'.
            this.addAt(item, itemIndex);
            if (previousSection !== modelItem.section) {
              item.setAttribute('section-start', modelItem.section);
            }
          }
          previousSection = modelItem.section;
        }
      }
      itemIndex++;
      modelIndex++;
    }
  }

  /**
   * Finds a parent directory of the {@code entry} in {@code this}, and
   * invokes the DirectoryItem.selectByEntry() of the found directory.
   *
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The entry to be searched
   *     for. Can be a fake.
   * @return {!Promise<boolean>} True if the parent item is found.
   */
  async searchAndSelectByEntry(entry) {
    // If the |entry| is same as one of volumes or shortcuts, select it.
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryTree'.
    for (let i = 0; i < this.items.length; i++) {
      // Skips the Drive root volume. For Drive entries, one of children of
      // Drive root or shortcuts should be selected.
      // @ts-ignore: error TS2339: Property 'items' does not exist on type
      // 'DirectoryTree'.
      const item = this.items[i];
      if (item instanceof DriveVolumeItem) {
        continue;
      }

      if (isSameEntry(item.entry, entry)) {
        await item.selectByEntry(entry);
        return true;
      }
    }
    // Otherwise, search whole tree.
    const found =
        await DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(
            this, entry);
    return found;
  }

  /**
   * Select the item corresponding to the given entry.
   * @param {!DirectoryEntry|!FilesAppDirEntry} entry The directory entry to be
   *     selected. Can be a fake.
   * @return {!Promise<void>}
   */
  async selectByEntry(entry) {
    if (this.selectedItem && isSameEntry(entry, this.selectedItem.entry)) {
      return;
    }

    if (await this.searchAndSelectByEntry(entry)) {
      return;
    }

    this.updateSubDirectories(false /* recursive */);
    const currentSequence = ++this.sequence_;
    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    if (!volumeInfo) {
      return;
    }
    volumeInfo.resolveDisplayRoot(
        async () => {
          if (this.sequence_ !== currentSequence) {
            return;
          }
          if (!(await this.searchAndSelectByEntry(entry))) {
            this.selectedItem = null;
          }
        },
        (error) => {
          console.warn('Failed to select by entry due to', error);
        });
  }

  /**
   * Activates the volume or the shortcut corresponding to the given index.
   * @param {number} index 0-based index of the target top-level item.
   * @return {boolean} True if one of the volume items is selected.
   */
  activateByIndex(index) {
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryTree'.
    if (index < 0 || index >= this.items.length) {
      return false;
    }

    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryTree'.
    this.items[index].selected = true;
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'DirectoryTree'.
    this.items[index].activate();
    return true;
  }

  /**
   * Retrieves the latest subdirectories and update them on the tree.
   *
   * @param {boolean} recursive True if the update is recursively.
   * @param {function()=} opt_callback Called when subdirectories are fully
   *     updated.
   */
  updateSubDirectories(recursive, opt_callback) {
    this.redraw(recursive);
    if (opt_callback) {
      opt_callback();
    }
  }

  /**
   * Redraw the list.
   * @param {boolean} recursive True if the update is recursively. False if the
   *     only root items are updated.
   */
  redraw(recursive) {
    this.updateSubElementsFromList(recursive);
  }

  /**
   * Invoked when the filter is changed.
   * @private
   */
  onFilterChanged_() {
    // Returns immediately, if the tree is hidden.
    if (this.hidden) {
      return;
    }

    this.redraw(true /* recursive */);
  }

  /**
   * Invoked when a directory is changed.
   * @param {!chrome.fileManagerPrivate.FileWatchEvent} event Event.
   * @private
   */
  onDirectoryContentChanged_(event) {
    if (event.eventType !== 'changed' || !event.entry) {
      return;
    }

    this.updateTreeByEntry_(/** @type{!Entry} */ (event.entry));
  }

  /**
   * Updates tree by entry.
   * @param {!Entry} entry A changed entry. Deleted entry is passed when watched
   *     directory is deleted.
   * @private
   */
  updateTreeByEntry_(entry) {
    // @ts-ignore: error TS2339: Property 'getDirectory' does not exist on type
    // 'FileSystemEntry'.
    entry.getDirectory(
        entry.fullPath, {create: false},
        () => {
          // If entry exists.
          // e.g. /a/b is deleted while watching /a.
          // @ts-ignore: error TS2339: Property 'items' does not exist on type
          // 'DirectoryTree'.
          for (let i = 0; i < this.items.length; i++) {
            // @ts-ignore: error TS2339: Property 'items' does not exist on type
            // 'DirectoryTree'.
            if (this.items[i] instanceof VolumeItem ||
                // @ts-ignore: error TS2339: Property 'items' does not exist on
                // type 'DirectoryTree'.
                this.items[i] instanceof EntryListItem) {
              // @ts-ignore: error TS2339: Property 'items' does not exist on
              // type 'DirectoryTree'.
              this.items[i].updateItemByEntry(entry);
            }
          }
        },
        () => {
          // If entry does not exist, try to get parent and update the subtree
          // by it. e.g. /a/b is deleted while watching /a/b. Try to update /a
          // in this
          //     case.
          entry.getParent(
              (parentEntry) => {
                this.updateTreeByEntry_(parentEntry);
              },
              // @ts-ignore: error TS6133: 'error' is declared but its value is
              // never read.
              (error) => {
                // If it fails to get parent, update the subtree by volume.
                // e.g. /a/b is deleted while watching /a/b/c. getParent of
                // /a/b/c
                //     fails in this case. We falls back to volume update.
                //
                // TODO(yawano): Try to get parent path also in this case by
                //     manipulating path string.
                const volumeInfo = this.volumeManager.getVolumeInfo(entry);
                if (!volumeInfo) {
                  return;
                }

                // @ts-ignore: error TS2339: Property 'items' does not exist on
                // type 'DirectoryTree'.
                for (let i = 0; i < this.items.length; i++) {
                  // @ts-ignore: error TS2339: Property 'items' does not exist
                  // on type 'DirectoryTree'.
                  if (this.items[i] instanceof VolumeItem &&
                      // @ts-ignore: error TS2339: Property 'items' does not
                      // exist on type 'DirectoryTree'.
                      this.items[i].volumeInfo === volumeInfo) {
                    // @ts-ignore: error TS2339: Property 'items' does not exist
                    // on type 'DirectoryTree'.
                    this.items[i].updateSubDirectories(true /* recursive */);
                  }
                }
              });
        });
  }

  /**
   * Invoked when the current directory is changed.
   * @param {!Event} event Event.
   * @private
   */
  async onCurrentDirectoryChanged_(event) {
    // Clear last active item; this is set by search temporarily disabling
    // highlight in the directory tree. When the user changes the directory and
    // search is active, the search closes and  attempts to restore last active
    // item, unless we clear it.
    this.lastActiveItem_ = null;
    // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on type
    // 'Event'.
    await this.selectByEntry(event.newDirEntry);

    // Update style of the current item as inactive.
    this.updateActiveItemStyle_(/*active=*/ false);
    this.activeItem_ = this.selectedItem;
    // Update style of the new current item as active.
    this.updateActiveItemStyle_(/*active=*/ true);
    this.updateSubDirectories(/*recursive=*/ false, () => {});
  }

  /**
   * Sets whether the active item is highlighted.
   * @param {boolean} highlighted If the active item should be highlighted.
   * @return {boolean} Whether the highlight was changed.
   * @private
   */
  setActiveItemHighlighted_(highlighted) {
    if (highlighted) {
      if (!this.lastActiveItem_) {
        return false;
      }
      this.activeItem_ = this.lastActiveItem_;
      this.lastActiveItem_ = null;
      this.updateActiveItemStyle_(true);
      return true;
    }
    // Make it not highlighted path.
    if (!this.updateActiveItemStyle_(false)) {
      return false;
    }
    this.lastActiveItem_ = this.activeItem_;
    this.activeItem_ = null;
    return true;
  }

  /**
   * Updates active items style to show it as active or not.
   * @param {boolean} active Whether to style active item as active or not.
   * @return {boolean} If style has been updated.
   */
  updateActiveItemStyle_(active) {
    if (!this.activeItem_) {
      return false;
    }
    if (active) {
      this.activeItem_.setAttribute(
          'aria-description', str('CURRENT_DIRECTORY_LABEL'));
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'DirectoryItem'.
      this.activeItem_.rowElement.setAttribute('active', '');
    } else {
      this.activeItem_.removeAttribute('aria-description');
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'DirectoryItem'.
      this.activeItem_.rowElement.removeAttribute('active');
    }
    return true;
  }

  /**
   * Invoked when the volume list or shortcut list is changed.
   * @private
   */
  onListContentChanged_() {
    this.updateSubDirectories(false, () => {
      // If no item is selected now, try to select the item corresponding to
      // current directory because the current directory might have been
      // populated in this tree in previous updateSubDirectories().
      if (!this.selectedItem) {
        const currentDir = this.directoryModel_.getCurrentDirEntry();
        if (currentDir) {
          this.selectByEntry(currentDir);
        }
      }
    });
  }

  /*
   * The directory tree does not support horizontal scrolling (by design), but
   * can gain a scrollLeft > 0, see crbug.com/1025581. Always clamp scrollLeft
   * back to 0 if needed. In RTL, the scrollLeft clamp is not 0: it depends on
   * the element scrollWidth and clientWidth per crbug.com/721759.
   */
  onTreeScrollEvent_() {
    if (this.scrollRAFActive_ === true) {
      return;
    }

    /**
     * True if a scroll RAF is active: scroll events are frequent and serviced
     * using RAF to throttle our processing of these events.
     * @type {boolean}
     */
    this.scrollRAFActive_ = true;

    window.requestAnimationFrame(() => {
      this.scrollRAFActive_ = false;
      if (document.documentElement.getAttribute('dir') === 'rtl') {
        const scrollRight = this.scrollWidth - this.clientWidth;
        if (this.scrollLeft !== scrollRight) {
          this.scrollLeft = scrollRight;
        }
      } else if (this.scrollLeft) {
        this.scrollLeft = 0;
      }
    });
  }

  /**
   * Updates the UI after the layout has changed, due to resize events from
   * the splitter or from the DOM window.
   */
  relayout() {
    dispatchSimpleEvent(this, 'relayout', true);
  }

  // DirectoryTree is always expanded.
  /** @return {boolean} */
  get expanded() {
    return true;
  }
  /**
   * @param {boolean} value Not used.
   */
  // @ts-ignore: error TS6133: 'value' is declared but its value is never read.
  set expanded(value) {}

  /**
   * The DirectoryModel this tree corresponds to.
   * @type {DirectoryModel}
   */
  get directoryModel() {
    return this.directoryModel_;
  }

  /**
   * The VolumeManager instance of the system.
   * @type {VolumeManager}
   */
  get volumeManager() {
    return this.volumeManager_;
  }

  /**
   * The reference to shared MetadataModel instance.
   * @type {!MetadataModel}
   */
  get metadataModel() {
    return this.metadataModel_;
  }

  set dataModel(dataModel) {
    if (!this.onListContentChangedBound_) {
      this.onListContentChangedBound_ = this.onListContentChanged_.bind(this);
    }

    if (this.dataModel_) {
      this.dataModel_.removeEventListener(
          'change', this.onListContentChangedBound_);
      this.dataModel_.removeEventListener(
          'permuted', this.onListContentChangedBound_);
    }
    this.dataModel_ = dataModel;
    dataModel.addEventListener('change', this.onListContentChangedBound_);
    dataModel.addEventListener('permuted', this.onListContentChangedBound_);
  }

  get dataModel() {
    return this.dataModel_;
  }
}

/**
 * Decorates an element.
 * @param {HTMLElement} el Element to be DirectoryTree.
 * @param {!DirectoryModel} directoryModel Current DirectoryModel.
 * @param {!VolumeManager} volumeManager VolumeManager of the system.
 * @param {!MetadataModel} metadataModel Shared MetadataModel instance.
 * @param {!FileOperationManager} fileOperationManager
 * @param {boolean} fakeEntriesVisible True if it should show the fakeEntries.
 */
DirectoryTree.decorate =
    (el, directoryModel, volumeManager, metadataModel, fileOperationManager,
     fakeEntriesVisible) => {
      // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
      // 'HTMLElement'.
      el.__proto__ = DirectoryTree.prototype;
      el.setAttribute('files-ng', '');
      Object.freeze(directorytree);

      /** @type {DirectoryTree} */ (el).decorateDirectoryTree(
          directoryModel, volumeManager, metadataModel, fileOperationManager,
          fakeEntriesVisible);

      // @ts-ignore: error TS2339: Property 'rowElementDepthStyleHandler' does
      // not exist on type 'HTMLElement'.
      el.rowElementDepthStyleHandler = directorytree.styleRowElementDepth;
    };

/** @type {?Menu} */
// @ts-ignore: error TS2565: Property 'contextMenuForSubitems' is used before
// being assigned.
DirectoryTree.prototype.contextMenuForSubitems;
Object.defineProperty(
    DirectoryTree.prototype, 'contextMenuForSubitems',
    getPropertyDescriptor('contextMenuForSubitems', PropertyKind.JS));

/** @type {?Menu} */
// @ts-ignore: error TS2565: Property 'contextMenuForRootItems' is used before
// being assigned.
DirectoryTree.prototype.contextMenuForRootItems;
Object.defineProperty(
    DirectoryTree.prototype, 'contextMenuForRootItems',
    getPropertyDescriptor('contextMenuForRootItems', PropertyKind.JS));

/** @type {?Menu} */
// @ts-ignore: error TS2565: Property 'disabledContextMenu' is used before being
// assigned.
DirectoryTree.prototype.disabledContextMenu;
Object.defineProperty(
    DirectoryTree.prototype, 'disabledContextMenu',
    getPropertyDescriptor('disabledContextMenu', PropertyKind.JS));

/**
 * Creates a new DirectoryItem based on |modelItem|.
 * @param {NavigationModelItem} modelItem, model that will determine the type of
 *     DirectoryItem to be created.
 * @param {!DirectoryTree} tree The tree to add the new DirectoryItem to.
 * @return {!TreeItem} a newly created instance of a
 *     DirectoryItem type.
 */
DirectoryTree.createDirectoryItem = (modelItem, tree) => {
  switch (modelItem.type) {
    case NavigationModelItemType.VOLUME:
      const volumeModelItem =
          /** @type {NavigationModelVolumeItem} */ (modelItem);
      if (volumeModelItem.volumeInfo.volumeType ===
          VolumeManagerCommon.VolumeType.DRIVE) {
        return new DriveVolumeItem(volumeModelItem, tree);
      } else {
        return new VolumeItem(volumeModelItem, tree);
      }
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.SHORTCUT:
      return new ShortcutItem(
          /** @type {!NavigationModelShortcutItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.RECENT:
      return new FakeItem(
          VolumeManagerCommon.RootType.RECENT,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.CROSTINI:
      return new FakeItem(
          VolumeManagerCommon.RootType.CROSTINI,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.GUEST_OS:
      return new FakeItem(
          VolumeManagerCommon.RootType.GUEST_OS,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.DRIVE:
      return new FakeItem(
          VolumeManagerCommon.RootType.DRIVE,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.ENTRY_LIST:
      const rootType = modelItem.section === NavigationSection.REMOVABLE ?
          VolumeManagerCommon.RootType.REMOVABLE :
          VolumeManagerCommon.RootType.MY_FILES;
      return new EntryListItem(
          rootType,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.ANDROID_APP:
      return new AndroidAppItem(
          /** @type {!NavigationModelAndroidAppItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
    case NavigationModelItemType.TRASH:
      return new EntryListItem(
          VolumeManagerCommon.RootType.TRASH,
          /** @type {!NavigationModelFakeItem} */ (modelItem), tree);
      // @ts-ignore: error TS7027: Unreachable code detected.
      break;
  }
  assertNotReached(`No DirectoryItem model: "${modelItem.type}"`);
  return /** @type {TreeItem} */ ({});
};

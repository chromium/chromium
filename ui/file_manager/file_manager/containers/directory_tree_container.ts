// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/ash/common/util.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {maybeShowTooltip} from '../common/js/dom_utils.js';
import {isEntryInsideComputers, isEntryInsideDrive, isEntryInsideMyDrive, isGrandRootEntryInDrives, isMyFilesEntry, isTrashEntry, isVolumeEntry} from '../common/js/entry_utils.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../common/js/files_app_entry_types.js';
import {metrics} from '../common/js/metrics.js';
import {str, strf} from '../common/js/util.js';
import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';
import {FileData, FileKey, NavigationKey, NavigationRoot, NavigationType, PropStatus, State} from '../externs/ts/state.js';
import {VolumeManager} from '../externs/volume_manager.js';
import {constants} from '../foreground/js/constants.js';
import {DirectoryModel} from '../foreground/js/directory_model.js';
import {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import {Command} from '../foreground/js/ui/command.js';
import {contextMenuHandler} from '../foreground/js/ui/context_menu_handler.js';
import {Menu} from '../foreground/js/ui/menu.js';
import {changeDirectory} from '../state/actions/current_directory.js';
import {refreshNavigationRoots, updateNavigationEntry} from '../state/actions/navigation.js';
import {readSubDirectories} from '../state/actions_producers/all_entries.js';
import {convertEntryToFileData} from '../state/reducers/all_entries.js';
import {driveRootEntryListKey} from '../state/reducers/volumes.js';
import {getEntry, getFileData, getStore, Store} from '../state/store.js';
import {TreeSelectedChangedEvent, XfTree} from '../widgets/xf_tree.js';
import {TreeItemCollapsedEvent, TreeItemExpandedEvent, XfTreeItem} from '../widgets/xf_tree_item.js';

/**
 * @fileoverview The Directory Tree aka Navigation Tree.
 * @suppress {checkTypes} TS already checks this file.
 */

const DRIVE_ENTRY_METADATA_PROPERTY_NAMES = [
  ...constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES,
];

const NAVIGATION_TYPES_WITHOUT_CHILDREN = new Set([
  NavigationType.ANDROID_APPS,
  NavigationType.RECENT,
  NavigationType.SHORTCUT,
  NavigationType.TRASH,
]);

/**
 * The navigation item data structure, which includes:
 * * `element`: the corresponding DOM element on the UI (rendered by
 * XfTreeItem).
 * * `fileData`: the corresponding file data which backs up this navigation
 * item.
 */
interface NavigationItemData {
  element: XfTreeItem;
  fileData: FileData|null;
}

/**
 * The navigation root data structure, which includes:
 * * `element` and `fileData`.
 * * `androidAppData`: the corresponding android app data which backs up this
 * navigation item, can be null if the navigation is not backed up by an
 * android app.
 */
interface NavigationRootItemData extends NavigationItemData {
  androidAppData: chrome.fileManagerPrivate.AndroidApp|null;
}

export class DirectoryTreeContainer {
  /** The root tree widget. */
  tree = document.createElement('xf-tree');
  /** Context menu element for root navigation items. */
  contextMenuForRootItems: Menu|null = null;
  /** Context menu element for sub navigation items. */
  contextMenuForSubitems: Menu|null = null;
  /** Context menu element for disabled navigation items. */
  contextMenuForDisabledItems: Menu|null = null;
  /**
   * Entry to be renamed. When this is set and the corresponding tree item is
   * rendered, we will attach a rename input inside the item.
   */
  entryKeyToRename: FileKey|null = null;

  private store_: Store;

  /**
   * The map of the navigation roots and items, from a navigation key to an
   * object which includes the navigation DOM element and the related data.
   *
   * Note: we are having 2 separate map for roots and items, because root item
   * and normal item can have the same key, e.g. for Shortcut item and its
   * original item, they share the same key but have different DOM elements, so
   * they can't be in the same map.
   */
  private navigationRootMap_ = new Map<NavigationKey, NavigationRootItemData>();
  private navigationItemMap_ = new Map<NavigationKey, NavigationItemData>();
  /**
   * Indicate if the RequestAnimationFrame is active for scroll or not, check
   * the usage in `onTreeScroll_` below.
   */
  private scrollRAFActive_ = false;

  /** Navigation roots from the store. */
  private navigationRoots_: State['navigation']['roots'] = [];
  /** Volumes data from the store. */
  private volumes_: State['volumes']|null = null;
  /** Folder shortcuts data from the store. */
  private folderShortcuts_: State['folderShortcuts']|null = null;
  /** UI entries data from the store. */
  private uiEntries_: State['uiEntries']|null = null;
  /** Android apps data from the store. */
  private androidApps_: State['androidApps']|null = null;

  constructor(
      container: HTMLElement, private directoryModel_: DirectoryModel,
      private volumeManager_: VolumeManager,
      private metadataModel_: MetadataModel) {
    this.tree.id = 'directory-tree';
    container.appendChild(this.tree);

    this.tree.addEventListener(
        XfTreeItem.events.TREE_ITEM_EXPANDED,
        this.onNavigationItemExpanded_.bind(this));
    this.tree.addEventListener(
        XfTreeItem.events.TREE_ITEM_COLLAPSED,
        this.onNavigationItemCollapsed_.bind(this));
    this.tree.addEventListener(
        XfTree.events.TREE_SELECTION_CHANGED,
        this.onNavigationItemSelected_.bind(this));
    this.tree.addEventListener(
        'mouseover', this.onTreeMouseOver_.bind(this), {passive: true});
    const fileFilter = this.directoryModel_.getFileFilter();
    fileFilter.addEventListener(
        'changed', this.onFileFilterChanged_.bind(this));
    this.tree.addEventListener(
        'scroll', this.onTreeScroll_.bind(this), {passive: true});
    // For file watcher.
    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        this.onFileWatcherEntryChanged_.bind(this));

    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  onStateChanged(state: State) {
    if (this.shouldRefreshNavigationRoots_(state)) {
      this.store_.dispatch(refreshNavigationRoots());
      // Skip this render, and the refreshNavigationRoots() action will trigger
      // another call of `onStateChanged`, which will run the re-render logic
      // below.
      return;
    }

    const {navigation: {roots}, androidApps, currentDirectory} = state;

    // When current directory changes in the store, select the corresponding
    // navigation item.
    if (currentDirectory?.key &&
        currentDirectory.status === PropStatus.SUCCESS) {
      const element =
          this.getNavigationDataFromKey(currentDirectory.key)?.element;
      if (element && !element.selected) {
        element.selected = true;
      }
    }

    // When navigation roots data changes in the store, re-render all navigation
    // root items.
    if (this.navigationRoots_ !== roots) {
      this.renderRoots_(roots);
    }

    // Navigation item can be backed up by either a FileData or a
    // AndroidAppData, we need to compare what we have in the container with the
    // data in the store to see if it changes or not. When
    // FileData/AndroidAppData changes in the store, re-render the corresponding
    // navigation item.
    for (const [key, {fileData, androidAppData}] of this.navigationRootMap_) {
      const newAndroidAppData = androidApps[key];
      const navigationRoot = this.navigationRoots_.find(
          navigationRoot => navigationRoot.key === key)!;
      if (navigationRoot.type === NavigationType.ANDROID_APPS) {
        if (androidAppData !== newAndroidAppData) {
          this.renderItem_(key, newAndroidAppData, navigationRoot);
        }
      } else {
        const newFileData = getFileData(state, key);
        if (fileData !== newFileData) {
          this.renderItem_(key, newFileData, navigationRoot);
        }
      }
    }
    for (const [key, {fileData}] of this.navigationItemMap_) {
      const newFileData = getFileData(state, key);
      if (fileData !== newFileData) {
        this.renderItem_(key, newFileData);
      }
    }
  }

  private renderRoots_(newRoots: NavigationRoot[]) {
    const newRootsSet = new Set(newRoots.map(root => root.key));
    // Remove non-exist navigation roots.
    for (const oldRoot of this.navigationRoots_) {
      if (!newRootsSet.has(oldRoot.key)) {
        this.deleteItem_(
            oldRoot.key, /* shouldDeleteElement= */ true, /* isRoot= */ true);
      }
    }

    // Add new navigation roots.
    const state = this.store_.getState();
    const {androidApps} = state;
    newRoots.forEach((navigationRoot, index) => {
      const exists = this.navigationRootMap_.has(navigationRoot.key);
      const navigationData = this.navigationRootMap_.get(navigationRoot.key)!;
      const navigationRootItem = exists ?
          navigationData.element :
          document.createElement('xf-tree-item');
      if (!exists) {
        this.navigationRootMap_.set(navigationRoot.key, {
          element: navigationRootItem,
          fileData: null,
          androidAppData: null,
        });
      }
      const isAndroidApp = navigationRoot.type === NavigationType.ANDROID_APPS;
      const fileData = getFileData(state, navigationRoot.key);
      const androidAppData = androidApps[navigationRoot.key];
      this.renderItem_(
          navigationRoot.key, isAndroidApp ? androidAppData : fileData,
          navigationRoot);
      // Always call insertBefore here even if the element already exists,
      // because the index can change. Calling insertBefore with existing
      // child element will move it to the correct position.
      this.tree.insertBefore(
          // Use `children` here because `items` is asynchronous.
          navigationRootItem, this.tree.children[index] || null);
      if (!exists && !isAndroidApp) {
        this.handleInitialRender_(navigationRootItem, fileData, navigationRoot);
      }
    });

    this.navigationRoots_ = newRoots;
  }

  private renderItem_(
      navigationKey: NavigationKey,
      newData: FileData|chrome.fileManagerPrivate.AndroidApp|null,
      navigationRoot?: NavigationRoot) {
    if (!newData) {
      // The corresponding data is deleted from the store, do nothing here.
      return;
    }

    const navigationData =
        this.getNavigationDataFromKey(navigationKey, !!navigationRoot)!;
    const {element} = navigationData;
    // We put the navigationKey on the element's dataset, so when certain DOM
    // events happens from the element, we know the corresponding navigation
    // key.
    element.dataset['navigationKey'] = navigationKey;
    const isAndroidApp = navigationRoot?.type === NavigationType.ANDROID_APPS;
    // Handle navigation items backed up by an android app. Note: only
    // navigation root item can be backed up by an android app.
    if (isAndroidApp) {
      if ((navigationData as NavigationRootItemData).androidAppData ===
          newData) {
        // Nothing changes, this render might be triggered by its parent.
        return;
      }
      const androidAppData = newData as chrome.fileManagerPrivate.AndroidApp;

      element.label = androidAppData.name;
      element.iconSet = androidAppData.iconSet || null;
      element.separator = navigationRoot.separator;
      // Setup external link for android app item.
      this.setupAndroidAppLink_(element);
      // Update new data back to the map.
      (navigationData as NavigationRootItemData).androidAppData =
          androidAppData;
      return;
    }

    // Handle navigation items backed up by a file entry.
    if (navigationData.fileData === newData) {
      // Nothing changes, this render might be triggered by its parent.
      return;
    }
    const fileData = newData as FileData;

    // TODO(b/228139439): The current menu/command implementation requires a
    // valid `.entry` existed on the tree item. We should remove this `.entry`
    // when refactoring the command part.
    (element as any).entry = fileData.entry;

    element.label = fileData.label;
    if (navigationRoot) {
      element.separator = navigationRoot.separator;
    }
    this.setItemIcon_(element, fileData, navigationRoot);
    element.disabled = fileData.disabled;

    // Add eject button for ejectable item.
    if (fileData.isEjectable) {
      this.setupEjectButton_(element, fileData.label);
    }

    // Fetch metadata if the entry supports Drive specific share icon.
    if (this.shouldSupportDriveSpecificIcons_(fileData)) {
      this.metadataModel_.get(
          [fileData.entry as Entry], DRIVE_ENTRY_METADATA_PROPERTY_NAMES);
    }

    if (!navigationRoot?.type ||
        !NAVIGATION_TYPES_WITHOUT_CHILDREN.has(navigationRoot.type)) {
      // Handle navigation item's children.
      const newChildren = fileData.children || [];
      // Remove non-exist navigation items.
      const newChildrenSet = new Set(newChildren);
      const oldChildren = navigationData.fileData?.children || [];
      for (const childKey of oldChildren) {
        if (!newChildrenSet.has(childKey)) {
          this.deleteItem_(
              childKey, /* shouldDeleteElement= */ true, /* isRoot= */ false);
        }
      }
      const state = this.store_.getState();
      newChildren.forEach((childKey, index) => {
        const exists = this.navigationItemMap_.has(childKey);
        const navigationData = this.navigationItemMap_.get(childKey)!;
        const navigationItem = exists ? navigationData.element :
                                        document.createElement('xf-tree-item');
        if (!exists) {
          this.navigationItemMap_.set(childKey, {
            element: navigationItem,
            fileData: null,
          });
        }
        const childFileData = getFileData(state, childKey);
        const isRenaming = navigationItem.renaming;
        this.renderItem_(childKey, childFileData);
        // Always call insertBefore here even if the element already exists,
        // because the index can change. Calling insertBefore with existing
        // child element will move it to the correct position.
        element.insertBefore(
            navigationItem,
            // Use `.children` instead of `.items` here because `items` is
            // asynchronous.
            element.children[index] || null);
        // `insertBefore` here will be called multiple times because the private
        // API `fileManagerPrivate.onDirectoryChanged` will be triggered more
        // than once. If the current item in in renaming process, the call will
        // blur the rename input, we need to resume the rename status here.
        if (isRenaming) {
          this.attachRename_(navigationItem);
        }

        if (!exists) {
          this.handleInitialRender_(navigationItem, childFileData);
        }
      });
    }

    if (this.entryKeyToRename === navigationKey) {
      this.entryKeyToRename = null;
      this.attachRename_(element);
    }

    // Update new data to the map.
    navigationData.fileData = fileData;
  }

  /**
   * Update navigation item icon based on the navigation data and the entry
   * data.
   */
  private setItemIcon_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot) {
    if (navigationRoot?.type === NavigationType.SHORTCUT) {
      element.icon = constants.ICON_TYPES.SHORTCUT;
      return;
    }
    // Navigation icon might be chrome.fileManagerPrivate.IconSet type.
    if (typeof fileData.icon === 'object') {
      element.iconSet = fileData.icon;
    } else {
      element.icon = fileData.icon;
    }
    // For drive item, update icon based on the metadata.
    if (this.shouldSupportDriveSpecificIcons_(fileData) && fileData.metadata) {
      const {shared, isMachineRoot, isExternalMedia} = fileData.metadata;
      if (shared) {
        element.icon = constants.ICON_TYPES.SHARED_FOLDER;
      }
      if (isMachineRoot) {
        element.icon = constants.ICON_TYPES.COMPUTER;
      }
      if (isExternalMedia) {
        element.icon = constants.ICON_TYPES.USB;
      }
    }
  }

  /** Append an eject button as the trailing slot of the navigation item. */
  private setupEjectButton_(element: XfTreeItem, label: string) {
    let ejectButton =
        element.querySelector('[slot=trailingIcon]') as CrButtonElement;
    if (!ejectButton) {
      ejectButton = document.createElement('cr-button');
      ejectButton.className = 'root-eject align-right-icon';
      ejectButton.slot = 'trailingIcon';
      ejectButton.tabIndex = 0;
      ejectButton.addEventListener('click', (event) => {
        event.stopPropagation();
        const command = document.querySelector('command#unmount') as Command;
        // Ensure 'canExecute' state of the command is properly setup for the
        // root before executing it.
        command.canExecuteChange(element);
        command.execute(element);
      });
      // Add icon.
      const ironIcon = document.createElement('iron-icon');
      ironIcon.setAttribute('icon', 'files20:eject');
      ejectButton.appendChild(ironIcon);

      element.appendChild(ejectButton);
    }
    ejectButton.ariaLabel = strf('UNMOUNT_BUTTON_LABEL', label);
  }

  /** Create an external link icon for android app navigation item.*/
  private setupAndroidAppLink_(element: XfTreeItem) {
    let externalLink =
        element.querySelector('[slot=trailingIcon]') as HTMLSpanElement;
    if (!externalLink) {
      // Use aria-describedby attribute to let ChromeVox users know that the
      // link launches an external app window.
      element.setAttribute('aria-describedby', 'external-link-label');

      // Create an external link.
      externalLink = document.createElement('span');
      externalLink.slot = 'trailingIcon';
      externalLink.className = 'external-link-icon align-right-icon';

      // Append external-link iron-icon.
      const ironIcon = document.createElement('iron-icon');
      ironIcon.setAttribute('icon', `files20:external-link`);
      externalLink.appendChild(ironIcon);

      element.appendChild(externalLink);
    }
  }

  /** Handle initial rendering. */
  private handleInitialRender_(
      element: XfTreeItem, fileData: FileData|null,
      navigationRoot?: NavigationRoot) {
    if (!fileData) {
      return;
    }
    this.setContextMenu_(element, fileData, navigationRoot);
    // Expand MyFiles by default.
    const entry = fileData.entry;
    if (isMyFilesEntry(entry)) {
      element.mayHaveChildren = true;
      element.expanded = true;
      this.store_.dispatch(updateNavigationEntry({
        key: entry.toURL(),
        expanded: true,
      }));
      return;
    }
    if (fileData.shouldDelayLoadingChildren) {
      // For navigation root items, even if its children loading should be
      // delayed, we don't show expand icon (e.g. SMB).
      if (!navigationRoot) {
        element.mayHaveChildren = true;
      }
      // For SMB shares, avoid prefetching sub directories to delay
      // authentication.
      if (isVolumeEntry(entry) && entry.volumeInfo.providerId !== '@smb' &&
          fileData.volumeType !== VolumeManagerCommon.VolumeType.SMB) {
        this.store_.dispatch(readSubDirectories(entry));
      }
      return;
    }

    if (this.shouldUpdateSubEntriesInitially_(
            element, fileData, navigationRoot)) {
      this.store_.dispatch(readSubDirectories(entry));
    }
  }

  /**
   * Should we update sub entries initially when the navigation item is
   * created.
   */
  private shouldUpdateSubEntriesInitially_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot): boolean {
    const entry = fileData.entry;
    // MyFile will be expanded by default, which will trigger a update for sub
    // entries, no need to update again.
    if (isMyFilesEntry(entry)) {
      return false;
    }
    // For curtain root types, we know there's no child navigation items.
    if (navigationRoot?.type &&
        NAVIGATION_TYPES_WITHOUT_CHILDREN.has(navigationRoot.type)) {
      return false;
    }
    // Only read the sub entries when it's the top level (where parentItem =
    // null) or its parent item has expanded.
    const parentItem = element.parentItem;
    if (!parentItem || parentItem.expanded) {
      return true;
    }
    return false;
  }

  /** Delete the navigation item by navigation key. */
  private deleteItem_(
      navigationKey: NavigationKey, shouldDeleteElement: boolean,
      isRoot: boolean) {
    const navigationData = this.getNavigationDataFromKey(navigationKey, isRoot);
    if (!navigationData) {
      console.warn(
          'Couldn\'t find the navigation data for the item to be deleted in the store.');
      return;
    }

    const {element, fileData} = navigationData;
    if (shouldDeleteElement && element.parentElement) {
      element.parentElement.removeChild(element);
    }
    if (isRoot) {
      this.navigationRootMap_.delete(navigationKey);
    } else {
      this.navigationItemMap_.delete(navigationKey);
    }
    // Also delete all the children keys from the map.
    if (fileData) {
      for (const childKey of fileData.children) {
        // For children element we don't need to explicitly delete DOM element
        // because removing their parent will also remove them implicitly.
        this.deleteItem_(
            childKey, /* shouldDeleteElement= */ false, /* isRoot= */ false);
      }
    }
  }

  /**
   * Returns true if fileData's entry supports the "shared" feature, as in,
   * displays a shared icon. It's only supported inside "My Drive" or
   * "Computers", even Shared Drive does not support it, the "My Drive" and
   * "Computers" itself don't support it either, only their children.
   *
   * Note: if the return value is true, fileData's entry is guaranteed to be
   * native Entry type.
   */
  private shouldSupportDriveSpecificIcons_(fileData: FileData): boolean {
    return (isEntryInsideMyDrive(fileData) && !isVolumeEntry(fileData.entry)) ||
        (isEntryInsideComputers(fileData) &&
         !isGrandRootEntryInDrives(fileData.entry));
  }

  /**
   * Given an navigation DOM element, find out the corresponding navigation
   * data in the root map or item map.
   */
  private getNavigationDataFromKey(
      navigationKey: NavigationKey, isRoot?: boolean): NavigationItemData|null {
    // If isRoot is passed, we know clearly which map to search.
    if (isRoot !== undefined) {
      const navigationData = isRoot ?
          this.navigationRootMap_.get(navigationKey) :
          this.navigationItemMap_.get(navigationKey);
      return navigationData || null;
    }

    // Checking if the navigationKey is a navigation root or a regular
    // navigation item.
    if (this.navigationRootMap_.has(navigationKey)) {
      return this.navigationRootMap_.get(navigationKey)!;
    }
    if (this.navigationItemMap_.has(navigationKey)) {
      return this.navigationItemMap_.get(navigationKey)!;
    }
    return null;
  }

  /** Handler for navigation item expanded. */
  private onNavigationItemExpanded_(event: TreeItemExpandedEvent) {
    const treeItem = event.detail.item;
    const navigationKey = treeItem.dataset['navigationKey']!;
    const navigationData = this.getNavigationDataFromKey(navigationKey);
    if (!navigationData || !navigationData.fileData) {
      console.warn(
          'Couldn\'t find the navigation data for the expanded item in the store.');
      return;
    }

    const {fileData} = navigationData;
    this.store_.dispatch(updateNavigationEntry({
      key: navigationKey,
      expanded: true,
    }));

    // UMA: expand time.
    const rootType = fileData.rootType ?? 'unknown';
    const metricName = `DirectoryTree.Expand.${rootType}`;
    this.recordUmaForItemExpandedOrCollapsed_(fileData);

    // Read child entries.
    this.store_.dispatch(
        readSubDirectories(fileData.entry, /* recursive= */ true, metricName));
  }

  /** Handler for navigation item collapsed. */
  private onNavigationItemCollapsed_(event: TreeItemCollapsedEvent) {
    const treeItem = event.detail.item;
    const navigationKey = treeItem.dataset['navigationKey']!;
    const navigationData = this.getNavigationDataFromKey(navigationKey);
    if (!navigationData || !navigationData.fileData) {
      console.warn(
          'Couldn\'t find the navigation data for the collapsed item in the store.');
      return;
    }

    const {fileData} = navigationData;
    if (fileData.expanded) {
      this.store_.dispatch(updateNavigationEntry({
        key: navigationKey,
        expanded: false,
      }));
    }

    this.recordUmaForItemExpandedOrCollapsed_(fileData);
    if (fileData.shouldDelayLoadingChildren) {
      // For file systems where it is performance intensive
      // to update recursively when items expand this proactively
      // collapses all children to avoid having to traverse large
      // parts of the tree when reopened.
      for (const item of treeItem.items) {
        if (item.expanded) {
          item.expanded = false;
        }
      }
    }
  }

  /** Handler for navigation item selected. */
  private onNavigationItemSelected_(event: TreeSelectedChangedEvent) {
    const {previousSelectedItem, selectedItem} = event.detail;
    if (previousSelectedItem) {
      previousSelectedItem.removeAttribute('aria-description');
    }
    if (!selectedItem) {
      return;
    }
    selectedItem.setAttribute(
        'aria-description', str('CURRENT_DIRECTORY_LABEL'));
    const navigationKey = selectedItem.dataset['navigationKey']!;
    // When the navigation item selection changed from the store (e.g. triggered
    // by other parts of the UI), we don't want to activate the directory again
    // because it's already activated.
    if (this.isCurrentDirectoryActive_(navigationKey)) {
      return;
    }
    const navigationData = this.getNavigationDataFromKey(navigationKey);
    if (!navigationData) {
      console.warn(
          'Couldn\'t find the navigation data for the selected item in the store.');
      return;
    }

    const isRoot = 'androidAppData' in navigationData;
    const {fileData} = navigationData;
    if (fileData) {
      this.recordUmaForItemSelected_(fileData);
    }
    this.activateDirectory_(
        selectedItem, isRoot, fileData,
        isRoot ? (navigationData as NavigationRootItemData).androidAppData :
                 null);
  }

  /** Handler for mouse move event inside the tree. */
  private onTreeMouseOver_(event: MouseEvent) {
    this.maybeShowToolTip_(event);
  }

  /** Handler for file filter changed event. */
  private onFileFilterChanged_() {
    // We don't know which entry is being impacted, we need to refresh all
    // entries we have in the map.
    this.store_.beginBatchUpdate();
    for (const navigationRoot of this.navigationRoots_) {
      if (NAVIGATION_TYPES_WITHOUT_CHILDREN.has(navigationRoot.type)) {
        continue;
      }
      const {fileData} = this.navigationRootMap_.get(navigationRoot.key)!;
      if (!fileData) {
        continue;
      }
      if (fileData.shouldDelayLoadingChildren && !fileData.expanded) {
        continue;
      }
      this.store_.dispatch(
          readSubDirectories(fileData.entry, /* recursive= */ true));
    }
    this.store_.endBatchUpdate();
  }

  /**
   * Handler for mouse move event inside the tree.
   *
   * The directory tree does not support horizontal scrolling (by design), but
   * can gain a scrollLeft > 0, see crbug.com/1025581. Always clamp scrollLeft
   * back to 0 if needed. In RTL, the scrollLeft clamp is not 0: it depends on
   * the element scrollWidth and clientWidth per crbug.com/721759.
   */
  private onTreeScroll_() {
    if (this.scrollRAFActive_ === true) {
      return;
    }

    /**
     * True if a scroll RAF is active: scroll events are frequent and serviced
     * using RAF to throttle our processing of these events.
     */
    this.scrollRAFActive_ = true;
    const tree = this.tree;

    window.requestAnimationFrame(() => {
      this.scrollRAFActive_ = false;
      if (isRTL()) {
        const scrollRight = tree.scrollWidth - tree.clientWidth;
        if (tree.scrollLeft !== scrollRight) {
          tree.scrollLeft = scrollRight;
        }
      } else if (tree.scrollLeft) {
        tree.scrollLeft = 0;
      }
    });
  }

  /**
   * Handler for FileWatcher's change event.
   */
  private onFileWatcherEntryChanged_(
      event: chrome.fileManagerPrivate.FileWatchEvent) {
    if (event.eventType !==
            chrome.fileManagerPrivate.FileWatchEventType.CHANGED ||
        !event.entry) {
      return;
    }

    this.updateTreeByEntry_(event.entry as DirectoryEntry);
  }

  /** Record UMA for item expanded or collapsed. */
  private recordUmaForItemExpandedOrCollapsed_(fileData: FileData) {
    const rootType = fileData.rootType ?? 'unknown';
    const level = fileData.isRootEntry ? 'TopLevel' : 'NonTopLevel';
    const metricName = `Location.OnEntryExpandedOrCollapsed.${level}`;
    metrics.recordEnum(
        metricName, rootType, VolumeManagerCommon.RootTypesForUMA);
  }

  /** Record UMA for tree item selected. */
  private recordUmaForItemSelected_(fileData: FileData) {
    const rootType = fileData.rootType ?? 'unknown';
    const level = fileData.isRootEntry ? 'TopLevel' : 'NonTopLevel';
    const metricName = `Location.OnEntrySelected.${level}`;
    metrics.recordEnum(
        metricName, rootType, VolumeManagerCommon.RootTypesForUMA);
  }

  /** Activate the directory behind the item. */
  private activateDirectory_(
      element: XfTreeItem, isRoot: boolean, fileData: FileData|null,
      androidAppData: chrome.fileManagerPrivate.AndroidApp|null) {
    if (androidAppData) {
      chrome.fileManagerPrivate.selectAndroidPickerApp(androidAppData, () => {
        if (chrome.runtime.lastError) {
          console.error(
              'selectAndroidPickerApp error: ',
              chrome.runtime.lastError.message);
        } else {
          window.close();
        }
      });
      return;
    }

    if (fileData) {
      const entry = fileData.entry;

      if (isRoot) {
        const navigationRootData = this.navigationRoots_.find(
            navigationRoot => navigationRoot.key === fileData.entry.toURL());
        if (navigationRootData?.type === NavigationType.DRIVE) {
          // If Drive fake root is selected and we expand it and go to the My
          // Drive (1st child) directly.
          if (!element.expanded) {
            element.expanded = true;
          }
          if (fileData.children && fileData.children.length > 0) {
            this.store_.dispatch(
                changeDirectory({toKey: fileData.children[0]!}));
          }
          return;
        }

        if (navigationRootData?.type === NavigationType.SHORTCUT) {
          const onEntryResolved = (resolvedEntry: Entry) => {
            metrics.recordUserAction('FolderShortcut.Navigate');
            this.store_.dispatch(
                changeDirectory({toKey: resolvedEntry.toURL()}));
          };
          // For shortcuts we already have an Entry, but it has to be resolved
          // again in case, it points to a non-existing directory.
          if (entry) {
            (window as any)
                .webkitResolveLocalFileSystemURL(
                    entry.toURL(), onEntryResolved,
                    () => {
                        // Error, the entry can't be re-resolved. It may
                        // happen
                        // for shortcuts whose targets got removed after
                        // resolving the Entry during initialization.
                        // TODO: what to do here?
                    });
          }
          return;
        }
      }

      // For delayed loading navigation items, read children when it's
      // selected.
      if (fileData.shouldDelayLoadingChildren &&
          fileData.children.length === 0) {
        this.store_.dispatch(readSubDirectories(fileData.entry));
      }

      this.store_.dispatch(changeDirectory({toKey: entry.toURL()}));
    }
  }

  /**
   * When drop target changes, this will be called with the drop target
   * element.
   */
  doDropTargetAction(element: XfTreeItem) {
    element.expanded = true;
  }

  private maybeShowToolTip_(event: MouseEvent) {
    const treeItem = event.target;
    if (!treeItem || !(treeItem instanceof XfTreeItem)) {
      return;
    }

    const labelElement =
        treeItem.shadowRoot!.querySelector<HTMLSpanElement>('.tree-label');
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, treeItem.label);
  }

  /**
   * Updates tree by entry.
   * `entry` A changed entry. Changed directory entry is passed when watched
   * directory is deleted.
   */
  private updateTreeByEntry_(entry: DirectoryEntry) {
    // TODO(b/271485133): Remove `getDirectory` call here and prevent
    // convertEntryToFileData() below.
    entry.getDirectory(
        entry.fullPath, {create: false},
        () => {
          // Can't rely on store data to get entry's rootType, if the entry is
          // grand root entry's first sub folder, the grand root entry might not
          // be the in the store yet.
          const fileData = convertEntryToFileData(entry);
          // If entry exists.
          // e.g. /a/b is deleted while watching /a.
          if (isEntryInsideDrive(fileData) && isGrandRootEntryInDrives(entry)) {
            // For grand root related changes, we need to re-read child
            // entries from the fake drive root level, because the grand root
            // might be show/hide based on if they have children or not.
            const driveRootEntry =
                getEntry(this.store_.getState(), driveRootEntryListKey)! as
                EntryList;
            this.store_.dispatch(readSubDirectories(driveRootEntry));
          } else {
            this.store_.dispatch(readSubDirectories(entry));
          }
        },
        () => {
          // If entry does not exist, try to get parent and update the subtree
          // by it. e.g. /a/b is deleted while watching /a/b. Try to update /a
          // in this case.
          entry.getParent(
              (parentEntry) => {
                this.store_.dispatch(readSubDirectories(parentEntry));
              },
              () => {
                // If it fails to get parent, update the subtree by volume.
                // e.g. /a/b is deleted while watching /a/b/c. getParent of
                // /a/b/c fails in this case. We falls back to volume update.
                //
                // TODO(yawano): Try to get parent path also in this case by
                // manipulating path string.

                const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
                if (!volumeInfo) {
                  return;
                }
                for (const root of this.navigationRoots_) {
                  if (root.type !== NavigationType.VOLUME) {
                    continue;
                  }
                  const {fileData} = this.navigationItemMap_.get(root.key)!;
                  if (fileData && fileData.entry instanceof VolumeEntry &&
                      fileData.entry.volumeInfo === volumeInfo) {
                    this.store_.dispatch(readSubDirectories(
                        fileData.entry, /* recursive= */ true));
                  }
                }
              });
        });
  }

  /**
   * Check if we need to dispatch an action to refresh navigation roots based
   * on the state.
   */
  private shouldRefreshNavigationRoots_(state: State): boolean {
    const {volumes, folderShortcuts, uiEntries, androidApps} = state;
    if (this.volumes_ !== volumes ||
        this.folderShortcuts_ !== folderShortcuts ||
        this.uiEntries_ !== uiEntries || this.androidApps_ !== androidApps) {
      this.volumes_ = volumes;
      this.folderShortcuts_ = folderShortcuts;
      this.uiEntries_ = uiEntries;
      this.androidApps_ = androidApps;
      return true;
    }

    return false;
  }

  /** Setup context menu for the given element. */
  private setContextMenu_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot) {
    // Trash is FakeEntry, but we still want to return menus for sub items.
    if (isTrashEntry(fileData.entry)) {
      if (this.contextMenuForSubitems) {
        contextMenuHandler.setContextMenu(element, this.contextMenuForSubitems);
      }
      return;
    }
    // Disable menus for disabled items and FakeEntry items.
    if (element.disabled || fileData.entry instanceof FakeEntryImpl) {
      if (this.contextMenuForDisabledItems) {
        contextMenuHandler.setContextMenu(
            element, this.contextMenuForDisabledItems);
        return;
      }
    }
    if (navigationRoot) {
      // For MyFiles, show normal file operations menu.
      if (isMyFilesEntry(fileData.entry)) {
        if (this.contextMenuForSubitems) {
          contextMenuHandler.setContextMenu(
              element, this.contextMenuForSubitems);
        }
        return;
      }
      // For other navigation roots, always show menus for root items, including
      // the removable entry list.
      if (this.contextMenuForRootItems) {
        contextMenuHandler.setContextMenu(
            element, this.contextMenuForRootItems);
      }
      return;
    }
    // For non-root navigation items, show menus for sub items.
    if (this.contextMenuForSubitems) {
      contextMenuHandler.setContextMenu(element, this.contextMenuForSubitems);
    }
  }

  private async attachRename_(element: XfTreeItem) {
    await element.updateComplete;
    window.fileManager.directoryTreeNamingController.attachAndStart(
        element, false, null);
  }

  /**
   * Given a NavigationKey, check if the entry it represents is the current
   * directory in the store or not.
   */
  private isCurrentDirectoryActive_(navigationKey: NavigationKey) {
    const {currentDirectory} = this.store_.getState();
    return currentDirectory?.key === navigationKey &&
        currentDirectory.status === PropStatus.SUCCESS;
  }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';

import {maybeShowTooltip} from '../common/js/dom_utils.js';
import {canHaveSubDirectories, isGrandRootEntryInDrive, isInsideDrive, isMyFilesFileData, isOneDrive, isOneDriveId, isRecentFileData, isTrashFileData, isVolumeFileData, shouldSupportDriveSpecificIcons} from '../common/js/entry_utils.js';
import {vmTypeToIconName} from '../common/js/icon_util.js';
import {recordEnum, recordUserAction} from '../common/js/metrics.js';
import {str, strf} from '../common/js/translations.js';
import {RootTypesForUMA, VolumeType} from '../common/js/volume_manager_types.js';
import {ICON_TYPES} from '../foreground/js/constants.js';
import type {DirectoryModel} from '../foreground/js/directory_model.js';
import type {Command} from '../foreground/js/ui/command.js';
import {contextMenuHandler} from '../foreground/js/ui/context_menu_handler.js';
import type {Menu} from '../foreground/js/ui/menu.js';
import {convertEntryToFileData, readSubDirectories, readSubDirectoriesToCheckDirectoryChildren, shouldDelayLoadingChildren, traverseAndExpandPathEntries, updateFileData} from '../state/ducks/all_entries.js';
import {changeDirectory} from '../state/ducks/current_directory.js';
import {refreshNavigationRoots} from '../state/ducks/navigation.js';
import {clearSearch} from '../state/ducks/search.js';
import {driveRootEntryListKey} from '../state/ducks/volumes.js';
import {type AndroidApp, type CurrentDirectory, type FileData, type FileKey, type NavigationKey, type NavigationRoot, NavigationType, PropStatus, SearchLocation, type State} from '../state/state.js';
import {getFileData, getStore, getVolume, type Store} from '../state/store.js';
import {type TreeSelectedChangedEvent, XfTree} from '../widgets/xf_tree.js';
import {type TreeItemCollapsedEvent, type TreeItemExpandedEvent, XfTreeItem} from '../widgets/xf_tree_item.js';

/**
 * @fileoverview The Directory Tree aka Navigation Tree.
 */

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
  androidAppData: AndroidApp|null;
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
   * Mark the tree item with a specific file key as to be renamed. When rename
   * is triggered from outside and the item to be renamed is yet to be rendered
   * (e.g. "New folder" command), we store the file key here in order to attach
   * the rename input to the tree item when it's rendered.
   */
  private fileKeyToRename_: FileKey|null = null;

  /**
   * Mark the tree item with a specific file key as to be focused. When we need
   * to change the focus to a tree item which is yet to be rendered (e.g. an
   * item is just renamed and the newly renamed item is not rendered yet), we
   * store the file key here in order to focus the tree item when it's
   * rendered.
   */
  private fileKeyToFocus_: FileKey|null = null;

  /**
   * Sometimes the selected item can be changed from outside (e.g. currently
   * selected directory item gets deleted, or operations from other place which
   * triggers the active directory change), if the selected item is also focused
   * before the change, we need to shift the focus to the newly selected item
   * after it's rendered. This flag is used to control that.
   */
  private shouldFocusOnNextSelectedItem_: boolean = false;

  /**
   * When current directory changes, if the corresponding tree item has not been
   * rendered yet, an asynchronous read-sub-directory action will be dispatched
   * to read the children until we could find the item. During this asynchronous
   * process, the current directory might change again (either manually by user
   * or other operations), we need this variable to see if we need to trigger
   * another read-sub-directories call or not.
   */
  private fileKeyToSelect_: FileKey|null = null;

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
  private materializedViews_: State['materializedViews'] = [];

  constructor(container: HTMLElement, private directoryModel_: DirectoryModel) {
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

  async onStateChanged(state: State) {
    if (this.shouldRefreshNavigationRoots_(state)) {
      this.store_.dispatch(refreshNavigationRoots());
      // Skip this render, and the refreshNavigationRoots() action will trigger
      // another call of `onStateChanged`, which will run the re-render logic
      // below.
      return;
    }

    const {navigation: {roots}, androidApps, currentDirectory} = state;

    if (this.shouldUnselectCurrentDirectoryItem_()) {
      this.tree.selectedItem = null;
    } else {
      // When current directory changes in the store, and the selected item in
      // the tree is different from that, select the corresponding navigation
      // item.
      const selectedItemKey = this.tree.selectedItem?.dataset['navigationKey'];
      if (currentDirectory?.key &&
          currentDirectory.status === PropStatus.SUCCESS &&
          currentDirectory.key !== selectedItemKey) {
        await this.selectCurrentDirectoryItem_(currentDirectory);
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
      const newAndroidAppData = androidApps[key]!;
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

  renameItemWithKeyWhenRendered(fileKey: FileKey) {
    this.fileKeyToRename_ = fileKey;
  }

  focusItemWithKeyWhenRendered(fileKey: FileKey) {
    this.fileKeyToFocus_ = fileKey;
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
    for (const [index, navigationRoot] of newRoots.entries()) {
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
        // We put the navigationKey on the element's dataset, so when certain
        // DOM events happens from the element, we know the corresponding
        // navigation key.
        navigationRootItem.dataset['navigationKey'] = navigationRoot.key;
      }
      const isAndroidApp = navigationRoot.type === NavigationType.ANDROID_APPS;
      const fileData = getFileData(state, navigationRoot.key);
      const androidAppData = androidApps[navigationRoot.key]!;
      // The states here might be lost after `insertBefore`, we need to store
      // it here and restore it later if needed.
      const isFocused = document.activeElement === navigationRootItem;
      const isRenaming = navigationRootItem.renaming;
      if (fileData && isVolumeFileData(fileData)) {
        const volume = getVolume(state, fileData);
        const isOneDriveRoot = volume && isOneDrive(volume);
        if (isOneDriveRoot) {
          navigationRootItem.toggleAttribute('one-drive', true);
        }
      }

      this.renderItem_(
          navigationRoot.key, isAndroidApp ? androidAppData : fileData,
          navigationRoot);

      // Skip `insertBefore` for the tree item if it's an existing item in
      // renaming state, otherwise it will interrupt user's input (via
      // triggering `blur` event). Even we try to re-attach the rename input
      // after `insertBefore`, it still interrupts user's input.
      if (!isRenaming) {
        // Always call insertBefore here even if the element already exists,
        // because the index can change. Calling insertBefore with existing
        // child element will move it to the correct position.
        this.tree.insertBefore(
            // Use `children` here because `items` is asynchronous.
            navigationRootItem, this.tree.children[index] || null);
      }
      // For existing items, `insertBefore` call above might make the element
      // lose some status (e.g. focus), check if we need to restore
      // them or not.
      if (exists) {
        if (isFocused && !fileData?.disabled) {
          this.restoreFocus_(navigationRootItem, /* isExisting= */ true);
        }
        continue;
      }
      // For newly rendered items, check if they are the next item to
      // focus.
      if (this.fileKeyToFocus_ === navigationRoot.key && !fileData?.disabled) {
        // Item with file key to focus is rendered for the first time (e.g.
        // right after rename finishes), focus on it.
        this.fileKeyToFocus_ = null;
        this.restoreFocus_(navigationRootItem, /* isExisting= */ false);
      }
      // No need to handle `fileKeyToRename_` here, because it's not allowed to
      // create a new folder in the directory tree root.

      if (!isAndroidApp) {
        this.handleInitialRender_(navigationRootItem, fileData, navigationRoot);
      }
    }

    this.navigationRoots_ = newRoots;
  }

  private renderItem_(
      navigationKey: NavigationKey, newData: FileData|AndroidApp|null,
      navigationRoot?: NavigationRoot) {
    if (!newData) {
      // The corresponding data is deleted from the store, do nothing here.
      return;
    }

    const navigationData =
        this.getNavigationDataFromKey_(navigationKey, !!navigationRoot)!;
    const {element} = navigationData;
    const isAndroidApp = navigationRoot?.type === NavigationType.ANDROID_APPS;
    // Handle navigation items backed up by an android app. Note: only
    // navigation root item can be backed up by an android app.
    if (isAndroidApp) {
      if ((navigationData as NavigationRootItemData).androidAppData ===
          newData) {
        // Nothing changes, this render might be triggered by its parent.
        return;
      }
      const androidAppData = newData as AndroidApp;

      element.label = androidAppData.name;
      if (typeof androidAppData.icon === 'object') {
        element.iconSet = androidAppData.icon;
      } else {
        element.icon = androidAppData.icon;
      }
      element.separator = navigationRoot.separator;
      // Setup external link for android app item.
      this.setupAndroidAppLink_(element);
      // Update new data back to the map.
      (navigationData as NavigationRootItemData).androidAppData =
          androidAppData;
      return;
    }

    // Handle navigation items backed up by a file data.
    if (navigationData.fileData === newData) {
      // Nothing changes, this render might be triggered by its parent.
      return;
    }
    const fileData = newData as FileData;
    if (window.IN_TEST) {
      this.addAttributesForTesting_(element, fileData, navigationRoot);
    }

    element.expanded = fileData.expanded;
    element.mayHaveChildren =
        fileData.children.length > 0 || fileData.canExpand;
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

    // Handle navigation item's children.
    // For disabled tree or collapsed items, we don't render any children
    // inside, but we always render children for Drive even it's collapsed.
    // TODO(b/308504417): remove the special case for Drive.
    const shouldRenderChildren = !fileData.disabled &&
        (fileData.expanded || fileData.key === driveRootEntryListKey);
    if (shouldRenderChildren) {
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
      for (const [index, childKey] of newChildren.entries()) {
        const exists = this.navigationItemMap_.has(childKey);
        const navigationData = this.navigationItemMap_.get(childKey)!;
        const navigationItem = exists ? navigationData.element :
                                        document.createElement('xf-tree-item');
        if (!exists) {
          this.navigationItemMap_.set(childKey, {
            element: navigationItem,
            fileData: null,
          });
          // We put the navigationKey on the element's dataset, so when certain
          // DOM events happens from the element, we know the corresponding
          // navigation key.
          navigationItem.dataset['navigationKey'] = childKey;
        }
        const childFileData = getFileData(state, childKey);
        // The states here might be lost after `insertBefore`, we need to store
        // it here and restore it later if needed.
        const isFocused = document.activeElement === navigationItem;
        const isRenaming = navigationItem.renaming;

        this.renderItem_(childKey, childFileData);

        // Skip `insertBefore` for the tree item if it's an existing item in
        // renaming state, otherwise it will interrupt user's input (via
        // triggering `blur` event). Even we try to re-attach the rename input
        // after `insertBefore`, it still interrupts user's input.
        if (!isRenaming) {
          // Always call insertBefore here even if the element already exists,
          // because the index can change. Calling insertBefore with existing
          // child element will move it to the correct position.
          element.insertBefore(
              navigationItem,
              // Use `.children` instead of `.items` here because `items` is
              // asynchronous.
              element.children[index] || null,
          );
        }
        // For existing items, `insertBefore` call above might make the element
        // lose some status (e.g. focus), check if we need to restore
        // them or not.
        if (exists) {
          if (isFocused && !childFileData?.disabled) {
            this.restoreFocus_(navigationItem, /* isExisting= */ true);
          }
          continue;
        }
        // For newly rendered items, check if they are the next item to
        // rename/focus.
        if (this.fileKeyToFocus_ === childKey && !childFileData?.disabled) {
          // Item with file key to focus is rendered for the first time (e.g.
          // right after rename finishes), focus on it.
          this.fileKeyToFocus_ = null;
          this.restoreFocus_(navigationItem, /* isExisting= */ false);
        }
        if (this.fileKeyToRename_ === childKey) {
          // Item with file key to rename is rendered for the first time (e.g.
          // "New folder" case), attach the rename input here.
          this.fileKeyToRename_ = null;
          this.attachRename_(navigationItem);
        }

        this.handleInitialRender_(navigationItem, childFileData);
      }
    }

    const isOdfs =
        isOneDriveId(getVolume(this.store_.getState(), fileData)?.providerId);
    if (isOdfs && fileData?.disabled) {
      // The entries under ODFS are not disabled recursively. Collapse ODFS when
      // it is disabled.
      element.expanded = false;
    }

    // Update new data to the map.
    navigationData.fileData = fileData;
  }

  /**
   * Update navigation item icon based on the navigation data and the file data.
   */
  private setItemIcon_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot) {
    if (navigationRoot?.type === NavigationType.SHORTCUT) {
      element.icon = ICON_TYPES.SHORTCUT;
      return;
    }
    // Navigation icon might be chrome.fileManagerPrivate.IconSet type.
    if (typeof fileData.icon === 'object') {
      element.iconSet = fileData.icon;
    } else {
      element.icon = fileData.icon;
    }
    // For drive item, update icon based on the metadata.
    if (shouldSupportDriveSpecificIcons(fileData) && fileData.metadata) {
      const {shared, isMachineRoot, isExternalMedia} = fileData.metadata;
      if (shared) {
        element.icon = ICON_TYPES.SHARED_FOLDER;
      }
      if (isMachineRoot) {
        element.icon = ICON_TYPES.COMPUTER;
      }
      if (isExternalMedia) {
        element.icon = ICON_TYPES.USB;
      }
    }
  }

  /** Add attributes for testing purpose. */
  private addAttributesForTesting_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot) {
    // Add full-path for all non-root items.
    if (!navigationRoot) {
      element.setAttribute('full-path-for-testing', fileData.fullPath);
    }
    if (!isVolumeFileData(fileData)) {
      return;
    }
    // Add volume-type for the root volume items.
    const volumeData = getVolume(this.store_.getState(), fileData);
    if (!volumeData) {
      return;
    }
    if (volumeData.volumeType === VolumeType.GUEST_OS) {
      element.setAttribute(
          'volume-type-for-testing', vmTypeToIconName(volumeData.vmType));
    } else {
      element.setAttribute('volume-type-for-testing', volumeData.volumeType);
    }
  }

  /** Append an eject button as the trailing slot of the navigation item. */
  private setupEjectButton_(element: XfTreeItem, label: string) {
    let ejectButton =
        element.querySelector<CrButtonElement>('[slot=trailingIcon]')!;
    if (!ejectButton) {
      ejectButton = document.createElement('cr-button');
      ejectButton.className = 'root-eject align-right-icon';
      ejectButton.slot = 'trailingIcon';
      ejectButton.tabIndex = 0;
      // These events propagation needs to be stopped otherwise ripple will show
      // on the tree item when the button is pressed.
      // Note: 'up/down' are events from <paper-ripple> component.
      const suppressedEvents = ['mouseup', 'mousedown', 'up', 'down'];
      suppressedEvents.forEach(event => {
        ejectButton.addEventListener(event, event => {
          event.stopPropagation();
        });
      });
      ejectButton.addEventListener('click', (event) => {
        event.stopPropagation();
        const command = document.querySelector<Command>('command#unmount')!;
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
        element.querySelector<HTMLSpanElement>('[slot=trailingIcon]');
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
    // Set context menu for the item.
    this.setContextMenu_(element, fileData, navigationRoot);
    // Expand MyFiles by default.
    if (isMyFilesFileData(this.store_.getState(), fileData)) {
      element.expanded = true;
      return;
    }
    // Check if we need to read sub directories to check directory children or
    // not, we are doing this to see if we need to show expand icon or not.
    let shouldCheckDirectoryChildren: boolean;
    if (navigationRoot) {
      // For navigation root items, we always check except for Shortcut items.
      shouldCheckDirectoryChildren =
          navigationRoot.type !== NavigationType.SHORTCUT;
    } else {
      // For other items, we only check if it's parent is expanded. Usually
      // non-root item's children directory will be checked when expanded, but
      // volume could be added when it's already expanded (e.g. Crostini/Android
      // can be mounted when MyFiles is expanded).
      shouldCheckDirectoryChildren = !!(element.parentItem?.expanded);
    }
    if (shouldCheckDirectoryChildren) {
      this.store_.dispatch(
          readSubDirectoriesToCheckDirectoryChildren(fileData.key));
    }
  }

  /** Delete the navigation item by navigation key. */
  private deleteItem_(
      navigationKey: NavigationKey, shouldDeleteElement: boolean,
      isRoot: boolean) {
    const navigationData =
        this.getNavigationDataFromKey_(navigationKey, isRoot);
    if (!navigationData) {
      console.warn(
          'Couldn\'t find the navigation data for the item to be deleted in the store.');
      return;
    }

    const {element, fileData} = navigationData;
    if (shouldDeleteElement && element.parentElement) {
      const isFocused = element === document.activeElement;
      const isSelected = element.selected;
      element.parentElement.removeChild(element);
      if (isFocused) {
        if (isSelected) {
          this.shouldFocusOnNextSelectedItem_ = true;
        } else {
          this.tree.focusedItem = this.tree.selectedItem;
          // The focus now already changes back to BODY because of the
          // `removeChild` above, we need to restore it back to the tree.
          this.tree.focus();
        }
      }
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
   * Given an navigation DOM element, find out the corresponding navigation
   * data in the root map or item map.
   */
  private getNavigationDataFromKey_(
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
    const navigationData = this.getNavigationDataFromKey_(navigationKey);
    if (!navigationData || !navigationData.fileData) {
      console.warn(
          'Couldn\'t find the navigation data for the expanded item in the store.');
      return;
    }

    const {fileData} = navigationData;
    this.store_.dispatch(updateFileData({
      key: navigationKey,
      partialFileData: {expanded: true},
    }));

    // UMA: expand time.
    const rootType = fileData.rootType ?? 'unknown';
    const metricName = `DirectoryTree.Expand.${rootType}`;
    this.recordUmaForItemExpandedOrCollapsed_(fileData);

    // Read child entries.
    this.store_.dispatch(
        readSubDirectories(fileData.key, /* recursive= */ true, metricName));
  }

  /** Handler for navigation item collapsed. */
  private onNavigationItemCollapsed_(event: TreeItemCollapsedEvent) {
    const treeItem = event.detail.item;
    const navigationKey = treeItem.dataset['navigationKey']!;
    const navigationData = this.getNavigationDataFromKey_(navigationKey);
    if (!navigationData || !navigationData.fileData) {
      console.warn(
          'Couldn\'t find the navigation data for the collapsed item in the store.');
      return;
    }

    const {fileData} = navigationData;
    if (fileData.expanded) {
      this.store_.dispatch(updateFileData({
        key: navigationKey,
        partialFileData: {expanded: false},
      }));
    }

    this.recordUmaForItemExpandedOrCollapsed_(fileData);
    if (shouldDelayLoadingChildren(fileData, this.store_.getState())) {
      // For file systems where it is performance intensive
      // to update recursively when items expand, this proactively
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
      // An unselected current directory item can be selected by:
      //  1. either change search location back from others to THIS_FOLDER.
      //  2. or users manually click the unselected current directory item to
      //  select it.
      // For 1, we don't need to clear the search, but for 2, we need to clear
      // the search, hence the check here.
      if (this.shouldUnselectCurrentDirectoryItem_()) {
        this.store_.dispatch(clearSearch());
      }
      return;
    }
    const navigationData = this.getNavigationDataFromKey_(navigationKey);
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
    // We don't know which file key is being impacted, we need to refresh all
    // entries we have in the map.
    this.store_.beginBatchUpdate();
    for (const navigationRoot of this.navigationRoots_) {
      if (navigationRoot.type === NavigationType.SHORTCUT) {
        continue;
      }
      const {fileData} = this.navigationRootMap_.get(navigationRoot.key)!;
      if (!fileData) {
        continue;
      }
      if (!canHaveSubDirectories(fileData)) {
        continue;
      }
      if (shouldDelayLoadingChildren(fileData, this.store_.getState()) &&
          !fileData.expanded) {
        continue;
      }
      this.store_.dispatch(
          readSubDirectories(fileData.key, /* recursive= */ true));
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
    recordEnum(metricName, rootType, RootTypesForUMA);
  }

  /** Record UMA for tree item selected. */
  private recordUmaForItemSelected_(fileData: FileData) {
    const rootType = fileData.rootType ?? 'unknown';
    const level = fileData.isRootEntry ? 'TopLevel' : 'NonTopLevel';
    const metricName = `Location.OnEntrySelected.${level}`;
    recordEnum(metricName, rootType, RootTypesForUMA);
  }

  /** Activate the directory behind the item. */
  private activateDirectory_(
      element: XfTreeItem, isRoot: boolean, fileData: FileData|null,
      androidAppData: AndroidApp|null) {
    if (androidAppData) {
      // Exclude "icon" filed before sending it to the API.
      const {icon: _, ...androidAppDataForApi} = androidAppData;
      chrome.fileManagerPrivate.selectAndroidPickerApp(
          androidAppDataForApi, () => {
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

    if (!fileData) {
      return;
    }

    const fileKey = fileData.key;

    const navigationRootData = isRoot ?
        this.navigationRoots_.find(
            navigationRoot => navigationRoot.key === fileKey) :
        undefined;
    // TODO(b/308504417): Remove the special case for Drive.
    if (navigationRootData?.type === NavigationType.DRIVE) {
      if (fileData.children.length === 0) {
        // Drive volume isn't not mounted, we can only change directory to the
        // fake drive root.
        this.store_.dispatch(changeDirectory({toKey: fileKey}));
      } else {
        // If Drive fake root is selected and it has Drive volume inside, we
        // expand it and go to the My Drive (1st child) directly.
        element.expanded = true;
        // If "Google Drive" item is the currently focused item, we also need to
        // set `shouldFocusOnNextSelectedItem_` flag to make sure the focus
        // shifts to My Drive when it's rendered after expanding.
        if (document.activeElement === element) {
          this.shouldFocusOnNextSelectedItem_ = true;
        }
        const myDriveKey = fileData.children[0]!;
        const isMyDriveActive = this.isCurrentDirectoryActive_(myDriveKey);
        // If My Drive is already active, dispatching the changeDirectory below
        // with STARTED status won't trigger a SUCCESS status in DirectoryModel
        // because toKey is the same with the current directory key in the
        // store. As we rely on the SUCCESS status to decide which tree item to
        // select, we need to dispatch a SUCCESS status changeDirectory action
        // in this case.
        this.store_.dispatch(changeDirectory({
          toKey: myDriveKey,
          status: isMyDriveActive ? PropStatus.SUCCESS : PropStatus.STARTED,
        }));
      }
      return;
    }

    if (navigationRootData?.type === NavigationType.SHORTCUT) {
      recordUserAction('FolderShortcut.Navigate');
    }

    // For delayed loading navigation items, read children when it's selected.
    if (shouldDelayLoadingChildren(fileData, this.store_.getState()) &&
        fileData.children.length === 0) {
      this.store_.dispatch(
          readSubDirectoriesToCheckDirectoryChildren(fileData.key));
    }

    this.store_.dispatch(changeDirectory({toKey: fileKey}));
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
          if (isInsideDrive(fileData) && isGrandRootEntryInDrive(entry)) {
            // For grand root related changes, we need to re-read child
            // entries from the fake drive root level, because the grand root
            // might be show/hide based on if they have children or not.
            this.store_.dispatch(readSubDirectories(driveRootEntryListKey));
          } else {
            this.store_.dispatch(readSubDirectories(entry.toURL()));
          }
        },
        () => {
          // If entry does not exist, try to get parent and update the subtree
          // by it. e.g. /a/b is deleted while watching /a/b. Try to update /a
          // in this case.
          entry.getParent(
              (parentEntry) => {
                this.store_.dispatch(readSubDirectories(parentEntry.toURL()));
              },
              () => {
                // If it fails to get parent, update the subtree by volume.
                // e.g. /a/b is deleted while watching /a/b/c. getParent of
                // /a/b/c fails in this case. We falls back to volume update.
                const state = this.store_.getState();
                const fileData = getFileData(state, entry.toURL());
                const volumeId = fileData?.volumeId;
                if (!volumeId) {
                  return;
                }
                for (const root of this.navigationRoots_) {
                  const {fileData} = this.navigationRootMap_.get(root.key)!;
                  if (fileData?.volumeId === volumeId) {
                    this.store_.dispatch(readSubDirectories(
                        fileData.key, /* recursive= */ true));
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
        this.uiEntries_ !== uiEntries || this.androidApps_ !== androidApps ||
        this.materializedViews_ !== state.materializedViews) {
      this.volumes_ = volumes;
      this.folderShortcuts_ = folderShortcuts;
      this.uiEntries_ = uiEntries;
      this.androidApps_ = androidApps;
      this.materializedViews_ = state.materializedViews;
      return true;
    }

    return false;
  }

  /** Setup context menu for the given element. */
  private setContextMenu_(
      element: XfTreeItem, fileData: FileData,
      navigationRoot?: NavigationRoot) {
    // Trash is FakeEntry, but we still want to return menus for sub items.
    if (isTrashFileData(fileData)) {
      if (this.contextMenuForSubitems) {
        contextMenuHandler.setContextMenu(element, this.contextMenuForSubitems);
      }
      return;
    }
    // Disable menus for disabled items and RECENT items.
    // NOTE: Drive shared with me and offline are marked as RECENT.
    if (element.disabled || isRecentFileData(fileData)) {
      if (this.contextMenuForDisabledItems) {
        contextMenuHandler.setContextMenu(
            element, this.contextMenuForDisabledItems);
        return;
      }
    }
    if (navigationRoot) {
      // For MyFiles, show normal file operations menu.
      if (isMyFilesFileData(this.store_.getState(), fileData)) {
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

  /**
   * Attach a rename input to the tree item.
   */
  private async attachRename_(element: XfTreeItem) {
    await element.updateComplete;
    // We need to focus the new folder item before renaming, the focus should
    // be back to this item after renaming finishes (controlled by
    // DirectoryTreeNamingController).
    this.tree.focusedItem = element;
    window.fileManager.directoryTreeNamingController.attachAndStart(
        element, false, null);
  }

  /**
   * Restore the focus to the tree item.
   */
  private async restoreFocus_(element: XfTreeItem, isExisting: boolean) {
    if (!isExisting) {
      // This focus() call below requires the tree item to finish the first
      // render, hence the await above.
      await element.updateComplete;
    }
    element.focus();
  }

  /**
   * Given a NavigationKey, check if the key is the current directory in the
   * store or not.
   */
  private isCurrentDirectoryActive_(navigationKey: NavigationKey) {
    const {currentDirectory} = this.store_.getState();
    return currentDirectory?.key === navigationKey &&
        currentDirectory.status === PropStatus.SUCCESS;
  }

  private async selectCurrentDirectoryItem_(currentDirectory: CurrentDirectory):
      Promise<void> {
    const currentDirectoryKey = currentDirectory.key;
    const navigationData = this.getNavigationDataFromKey_(currentDirectoryKey);
    if (navigationData) {
      const element = navigationData.element;
      if (element && !element.selected) {
        // Reset fileKeyToSelect_ because we already find the element which
        // represents current directory.
        this.fileKeyToSelect_ = null;
        element.selected = true;
        // We only focus the element if shouldFocusOnNextSelectedItem_ is true.
        // This is because current directory change can't be triggered from
        // other parts of Files UI, e.g. "Go to file location" in Recents, where
        // we shouldn't steal the focus from others.
        if (this.shouldFocusOnNextSelectedItem_) {
          this.shouldFocusOnNextSelectedItem_ = false;
          // Wait for the selected change finishes (e.g. expand all its parents)
          // before we can focus on the element below.
          await element.updateComplete;
          element.focus();
        }
      }
      return;
    }
    // The item which represents the current directory can not be found in the
    // tree, we need to read sub directory from the root recursively until we
    // find the targeted current directory.

    if (this.fileKeyToSelect_ === currentDirectoryKey) {
      // Do nothing because we already started a reading call to find this exact
      // same "current directory" (see logic below.)
      return;
    }

    // Set the selected item to null before scanning for the target directory,
    // if we couldn't find the target directory after scanning (e.g. "Go to
    // file location" for Play files in recent view b/265101238), nothing
    // should be selected in the tree.
    this.tree.selectedItem = null;

    this.fileKeyToSelect_ = currentDirectoryKey;
    const pathKeys =
        currentDirectory.pathComponents.map(pathComponent => pathComponent.key);
    this.store_.dispatch(traverseAndExpandPathEntries(pathKeys));
  }

  /**
   * Check if we need to unselect the current directory item in the tree.
   * When searching is active and we are not searching current folder, we
   * shouldn't have any tree item selected.
   */
  private shouldUnselectCurrentDirectoryItem_(): boolean {
    const state = this.store_.getState();
    const {search, currentDirectory} = state;
    const isSearchActive = search?.status !== undefined && !!(search?.query);
    let isCurrentDirectoryInsideDrive = false;
    if (currentDirectory?.key) {
      const currentDirectoryData = getFileData(state, currentDirectory.key);
      // The current directory might not exist if it unmounts.
      if (currentDirectoryData) {
        isCurrentDirectoryInsideDrive = isInsideDrive(currentDirectoryData);
      }
    }
    const isSearchInCurrentFolder =
        // When searching in Drive, the search location option will only include
        // ROOT_FOLDER ("Google Drive"), not include THIS_FOLDER.
        (isCurrentDirectoryInsideDrive &&
         search?.options?.location === SearchLocation.ROOT_FOLDER) ||
        search?.options?.location === SearchLocation.THIS_FOLDER;
    return isSearchActive && !isSearchInCurrentFolder;
  }
}

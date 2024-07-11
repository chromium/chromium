// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ElementObject} from '../../prod/file_manager/shared_types.js';
import {getCaller, pending, repeatUntil} from '../../test_util.js';
import {remoteCall} from '../background.js';


const FAKE_ENTRY_PATH_PREFIX = 'fake-entry:';
const ENTRY_LIST_PATH_PREFIX = 'entry-list:';
const REAL_ENTRY_PATH_PREFIX = 'filesystem:chrome://file-manager/external';

/** This serves as the additional selector of the tree item. */
interface ModifierOptions {
  expanded?: boolean;
  selected?: boolean;
  focused?: boolean;
  shortcut?: boolean;
  renaming?: boolean;
  acceptDrop?: boolean;
  hasChildren?: boolean;
  mayHaveChildren?: boolean;
  currentDirectory?: boolean;
}

/**
 * Page object for Directory Tree, this class abstracts all the selectors
 * related to directory tree and its tree items.
 */
export class DirectoryTreePageObject {
  /**
   * Return a singleton instance of DirectoryTreePageObject. This will make sure
   * the directory tree DOM element is ready.
   */
  static async create(appId: string): Promise<DirectoryTreePageObject> {
    const directoryTree = new DirectoryTreePageObject(appId);
    await remoteCall.waitForElement(appId, directoryTree.rootSelector);
    return directoryTree;
  }

  private selectors_: DirectoryTreeSelectors;

  /**
   * Note: do not use constructor directly, use `create` static method instead,
   * which will fetch the `useNewTree_` value and make sure the tree DOM element
   * is ready.
   */
  constructor(private appId_: string) {
    this.selectors_ = new DirectoryTreeSelectors();
  }

  /**
   * Returns the selector for the tree root.
   */
  get rootSelector(): string {
    return this.selectors_.root;
  }

  /**
   * Returns the selector for the tree container.
   */
  get containerSelector(): string {
    return this.selectors_.container;
  }

  /**
   * Returns the selector by the tree label.
   *
   * @param label Label of the tree item
   */
  itemSelectorByLabel(label: string): string {
    return this.selectors_.itemByLabel(label);
  }

  /**
   * Wait for the selected(aka "active" in the old tree implementation) tree
   * item with the label.
   *
   * @param label Label of the tree item.
   */
  async waitForSelectedItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {selected: true}));
  }

  /**
   * Wait for the selected(aka "active" in the old tree implementation) tree
   * item with the label to be lost.
   *
   * @param label Label of the tree item.
   */
  async waitForSelectedItemLostByLabel(label: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_, this.selectors_.itemByLabel(label, {selected: true}));
  }

  /**
   * Wait for the tree item with the label to have focused (aka "selected" in
   * the old tree implementation) state.
   *
   * @param label Label of the tree item
   */
  async waitForFocusedItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {focused: true}));
  }

  /**
   * Wait for the tree item with the label to be focusable (aka "selected" in
   * the old tree implementation).
   *
   * @param label Label of the tree item
   */
  async waitForFocusableItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        // Go inside shadow DOM to check tabindex.
        this.appId_, [this.selectors_.itemByLabel(label), 'li[tabindex="0"]']);
  }

  /**
   * Wait for the tree item with the type to have focused (aka "selected" in the
   * old tree implementation) state.
   *
   * @param type Type of the tree item.
   */
  async waitForFocusedItemByType(type: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByType(
            type, /* isPlaceholder= */ false, {focused: true}));
  }

  /**
   * Wait for the shortcut tree item with the label to have focused (aka
   * "selected" in the old tree implementation) state.
   *
   * @param label Label of the tree item
   */
  async waitForFocusedShortcutItemByLabel(label: string):
      Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {focused: true, shortcut: true}));
  }

  /**
   * Wait for the tree item with the label to have the the current directory
   * aria-description attribute.
   *
   * @param label Label of the tree item
   */
  async waitForCurrentDirectoryItemByLabel(label: string):
      Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {currentDirectory: true}));
  }

  /**
   * Wait for the child items of the specific parent item to match the count.
   *
   * @param parentLabel Label of the parent tree item.
   * @param count Expected number of the child items.
   * @param excludeEmptyChild Set true to only return child items with nested
   *     children.
   */
  async waitForChildItemsCountByLabel(
      parentLabel: string, count: number,
      excludeEmptyChild?: boolean): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(parentLabel);
    const childItemsSelector = excludeEmptyChild ?
        this.selectors_.nonEmptyChildItems(itemSelector) :
        this.selectors_.childItems(itemSelector);
    return remoteCall.waitForElementsCount(
        this.appId_, [childItemsSelector], count);
  }

  /**
   * Wait for the placeholder tree items specified by type to match the count.
   *
   * @param type Type of the placeholder tree item.
   * @param count Expected number of the child items.
   */
  async waitForPlaceholderItemsCountByType(type: string, count: number):
      Promise<void> {
    const itemSelector =
        this.selectors_.itemByType(type, /* isPlaceholder= */ true);
    return remoteCall.waitForElementsCount(this.appId_, [itemSelector], count);
  }

  /** Get the currently focused tree item. */
  async getFocusedItem(): Promise<null|ElementObject> {
    const focusedItemSelector = this.selectors_.attachModifier(
        `${this.selectors_.root} ${this.selectors_.item}`, {focused: true});
    const elements = await remoteCall.callRemoteTestUtil<ElementObject[]>(
        'deepQueryAllElements', this.appId_, [focusedItemSelector]);
    if (elements && elements.length > 0) {
      return elements[0]!;
    }
    return null;
  }

  /** Gets the label of the tree item. */
  getItemLabel(item: ElementObject|null): string {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
    }
    return item.attributes['label']!;
  }

  /** Gets the volume type of the tree item. */
  getItemVolumeType(item: ElementObject|null): string {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
    }
    return item.attributes['volume-type-for-testing']!;
  }

  /** Check if the tree item is disabled or not. */
  assertItemDisabled(item: ElementObject|null) {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
    }
    // Empty value for "disabled" means it's disabled.
    chrome.test.assertEq('', item.attributes['disabled']);
  }

  /**
   * Wait for the item with the label to get the `has-children` attribute with
   * the specified value.
   *
   * @param label Label of the tree item.
   * @param hasChildren should the tree item have children or not.
   */
  async waitForItemToHaveChildrenByLabel(label: string, hasChildren: boolean):
      Promise<ElementObject> {
    // Expand the item first before checking its children.
    if (hasChildren) {
      await this.expandTreeItemByLabel(label);
    }
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {hasChildren: hasChildren}));
  }

  /**
   * Wait for the item with the type to get the `has-children` attribute with
   * the specified value.
   *
   * @param type Type of the tree item.
   * @param hasChildren should the tree item have children or not.
   */
  async waitForItemToHaveChildrenByType(type: string, hasChildren: boolean):
      Promise<ElementObject> {
    // Expand the item first before checking its children.
    if (hasChildren) {
      await this.expandTreeItemByType(type);
    }
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByType(
            type, /* isPlaceholder= */ false, {hasChildren: hasChildren}));
  }

  /**
   * Wait for the item with the label to get the `may-have-children` attribute.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToMayHaveChildrenByLabel(label: string):
      Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {mayHaveChildren: true}));
  }

  /**
   * Wait for the item with the label to be expanded.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToExpandByLabel(label: string): Promise<void> {
    const expandedItemSelector =
        this.selectors_.itemByLabel(label, {expanded: true});
    await remoteCall.waitForElement(this.appId_, expandedItemSelector);
  }

  /**
   * Wait for the item with the label to be collapsed.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToCollapseByLabel(label: string): Promise<void> {
    const collapsedItemSelector =
        this.selectors_.itemByLabel(label, {expanded: false});
    await remoteCall.waitForElement(this.appId_, collapsedItemSelector);
  }

  /**
   * Expands a single tree item with the specified label by clicking on its
   * expand icon.
   * @param label Label of the tree item we want to expand on.
   * @param allowEmpty Allow expanding tree item without any children.
   */
  async expandTreeItemByLabel(label: string, allowEmpty?: boolean):
      Promise<void> {
    await this.expandTreeItem_(this.selectors_.itemByLabel(label), allowEmpty);
  }

  /**
   * Expands a single tree item with the specified type by clicking on its
   * expand icon.
   * @param type Type of the tree item we want to expand on.
   * @param allowEmpty Allow expanding tree item without any children.
   */
  async expandTreeItemByType(type: string, allowEmpty?: boolean):
      Promise<void> {
    await this.expandTreeItem_(this.selectors_.itemByType(type), allowEmpty);
  }

  /**
   * Expands a single tree item with the specified full path by clicking on its
   * expand icon.
   * @param path Path of the tree item we want to expand on.
   */
  async expandTreeItemByPath(path: string): Promise<void> {
    await this.expandTreeItem_(this.selectors_.itemByPath(path));
  }

  /**
   * Collapses a single tree item with the specified label by clicking on its
   * expand icon.
   * @param label Label of the tree item we want to collapse on.
   */
  async collapseTreeItemByLabel(label: string): Promise<void> {
    await this.collapseTreeItem_(this.selectors_.itemByLabel(label));
  }

  /**
   * Expands each directory in the breadcrumbs path.
   *
   * @param breadcrumbsPath Path based in the entry labels like:
   *     /My files/Downloads/photos.
   * @return Promise fulfilled on success with the selector query of the last
   *    directory expanded.
   */
  async recursiveExpand(breadcrumbsPath: string): Promise<string> {
    const paths = breadcrumbsPath.split('/').filter(path => path);

    // Expand each directory in the breadcrumb.
    let query = this.selectors_.root;
    for (const parentLabel of paths) {
      // Wait for parent element to be displayed.
      query += ` ${this.selectors_.itemItselfByLabel(parentLabel)}`;
      await remoteCall.waitForElement(this.appId_, query);

      // Only expand if element isn't expanded yet.
      const elements = await remoteCall.callRemoteTestUtil<ElementObject[]>(
          'queryAllElements', this.appId_,
          [this.selectors_.attachModifier(query, {expanded: true})]);
      if (elements.length === 0) {
        await this.expandTreeItem_(query);
      }
    }

    return Promise.resolve(query);
  }

  /**
   * Focus the directory tree and navigates using mouse clicks.
   *
   * @param breadcrumbsPath Path based on the entry labels like:
   *     /My files/Downloads/photos to item that should navigate to.
   * @param shortcutToPath For shortcuts it navigates to a different breadcrumbs
   *     path, like /My Drive/ShortcutName.
   * @return the final selector used to click on the desired tree item.
   */
  async navigateToPath(breadcrumbsPath: string, shortcutToPath?: string):
      Promise<string> {
    // Focus the directory tree.
    await this.focusTree();

    const paths = breadcrumbsPath.split('/');
    // For "/My Drive", expand the "Google Drive" first.
    if (paths[1] === 'My Drive') {
      paths.unshift('', 'Google Drive');
    }
    const leaf = paths.pop()!;

    // Expand all parents of the leaf entry.
    let query = await this.recursiveExpand(paths.join('/'));

    // Navigate to the final entry.
    query += ` ${this.selectors_.itemItselfByLabel(leaf)}`;
    await remoteCall.waitAndClickElement(this.appId_, query);

    // Wait directory to finish scanning its content.
    await remoteCall.waitForElement(this.appId_, `[scan-completed="${leaf}"]`);

    // If the search was not closed, wait for it to close.
    await remoteCall.waitForElement(this.appId_, '#search-wrapper[collapsed]');

    // Wait to navigation to final entry to finish.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        this.appId_, (shortcutToPath || breadcrumbsPath));

    // Focus the directory tree.
    await this.focusTree();

    return query;
  }

  /**
   * Trigger a keydown event with ArrowUp key to move the focus to the previous
   * tree item.
   */
  async focusPreviousItem(): Promise<void> {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const arrowUp =
        [this.selectors_.keyboardRecipient, 'ArrowUp', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', this.appId_, arrowUp);
  }

  /**
   * Trigger a keydown event with ArrowDown key to move the focus to the next
   * tree item.
   *
   */
  async focusNextItem(): Promise<void> {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const arrowUp =
        [this.selectors_.keyboardRecipient, 'ArrowDown', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', this.appId_, arrowUp);
  }

  /**
   * Trigger a keydown event with Enter key to select currently focused item.
   *
   */
  async selectFocusedItem(): Promise<void> {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const enter =
        [this.selectors_.keyboardRecipient, 'Enter', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', this.appId_, enter);
  }

  /**
   * Wait for the tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async waitForItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label));
  }

  /**
   * Wait for the tree item by its label to be lost.
   *
   * @param label Label of the tree item.
   */
  async waitForItemLostByLabel(label: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_, this.selectors_.itemByLabel(label));
  }

  /**
   * Wait for the tree item by its full path.
   *
   * @param path Path of the tree item.
   */
  async waitForItemByPath(path: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.itemByPath(path));
  }

  /**
   * Wait for the tree item by its full path to be lost.
   *
   * @param path Path of the tree item.
   */
  async waitForItemLostByPath(path: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_, this.selectors_.itemByPath(path));
  }

  /** Returns the labels for all visible tree items. */
  async getVisibleItemLabels(): Promise<string[]> {
    const allItems = await remoteCall.callRemoteTestUtil<ElementObject[]>(
        'queryAllElements', this.appId_, [
          `${this.selectors_.root} ${this.selectors_.item}`,
          ['visibility'],
        ]);
    return allItems
        .filter(item => !item.hidden && item.styles!['visibility'] !== 'hidden')
        .map(item => this.getItemLabel(item));
  }

  /**
   * Wait for the tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async waitForItemByType(type: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Wait for the tree item by its type to be lost.
   *
   * @param type Type of the tree item.
   */
  async waitForItemLostByType(type: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Wait for the placeholder tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async waitForPlaceholderItemByType(type: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Wait for the placeholder tree item by its type to be lost.
   *
   * @param type Type of the tree item.
   */
  async waitForPlaceholderItemLostByType(type: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Wait for the shortcut tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async waitForShortcutItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Wait for the shortcut tree item by its label to be lost.
   *
   * @param label Label of the tree item.
   */
  async waitForShortcutItemLostByLabel(label: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_, this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Wait for the child tree item under a specified parent item by their label.
   *
   * @param parentLabel Label of the parent item.
   * @param childLabel Label of the child item.
   */
  async waitForChildItemByLabel(parentLabel: string, childLabel: string):
      Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.childItem(
            this.selectors_.itemByLabel(parentLabel),
            this.selectors_.itemItselfByLabel(childLabel)));
  }

  /**
   * Wait for the child tree item to be lost under a specified parent item by
   * its label.
   *
   * @param parentLabel Label of the parent item.
   * @param childLabel Label of the child item.
   */
  async waitForChildItemLostByLabel(parentLabel: string, childLabel: string):
      Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_,
        this.selectors_.childItem(
            this.selectors_.itemByLabel(parentLabel),
            this.selectors_.itemItselfByLabel(childLabel)));
  }

  /**
   * Wait for the group root tree item (e.g. entry list) by its type.
   *
   * @param type Type of the tree item.
   */
  async waitForGroupRootItemByType(type: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_, this.selectors_.groupRootItemByType(type));
  }

  /**
   * Returns the child items of a parent item specified by its label.
   *
   * @param parentLabel Label of the parent item.
   */
  async getChildItemsByParentLabel(parentLabel: string):
      Promise<ElementObject[]> {
    const parentItemSelector = this.selectors_.itemByLabel(parentLabel);
    const childItemsSelector = this.selectors_.childItems(parentItemSelector);
    return remoteCall.callRemoteTestUtil(
        'queryAllElements', this.appId_, [childItemsSelector]);
  }

  /**
   * Wait for the eject button under the tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async waitForItemEjectButtonByType(type: string): Promise<ElementObject> {
    return remoteCall.waitForElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Wait for the eject button to be lost under the tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async waitForItemEjectButtonLostByType(type: string): Promise<void> {
    await remoteCall.waitForElementLost(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Click the eject button under the tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async ejectItemByType(type: string): Promise<ElementObject> {
    return remoteCall.waitAndClickElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Click the eject button under the tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async ejectItemByLabel(label: string): Promise<ElementObject> {
    return remoteCall.waitAndClickElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByLabel(label)));
  }

  /**
   * Wait for the expand icon under the tree item to show by its label.
   *
   * @param label Label of the tree item.
   */
  async waitForItemExpandIconToShowByLabel(label: string): Promise<void> {
    const expandIcon =
        this.selectors_.expandIcon(this.selectors_.itemByLabel(label));
    const caller = getCaller();
    return repeatUntil(async () => {
      const element = await remoteCall.waitForElementStyles(
          this.appId_,
          expandIcon,
          ['visibility'],
      );
      if (element.styles!['visibility'] !== 'visible') {
        return pending(
            caller, `Expand icon for tree item ${label} is still hidden.`);
      }
      return undefined;
    });
  }

  /**
   * Wait for the expand icon under the tree item to hide by its label.
   *
   * @param label Label of the tree item.
   */
  async waitForItemExpandIconToHideByLabel(label: string): Promise<void> {
    const expandIcon =
        this.selectors_.expandIcon(this.selectors_.itemByLabel(label));
    const caller = getCaller();
    return repeatUntil(async () => {
      const element = await remoteCall.waitForElementStyles(
          this.appId_,
          expandIcon,
          ['visibility'],
      );
      if (element.styles!['visibility'] !== 'hidden') {
        return pending(
            caller, `Expand icon for tree item ${label} is still showing.`);
      }
      return undefined;
    });
  }

  /**
   * Wait for the tree item specified by label to accept drag/drop.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToAcceptDropByLabel(label: string): Promise<void> {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await remoteCall.waitForElement(this.appId_, itemAcceptDrop);
    await remoteCall.waitForElementLost(this.appId_, itemDenyDrop);
  }

  /**
   * Wait for the tree item specified by label to deny drag/drop.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToDenyDropByLabel(label: string): Promise<void> {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await remoteCall.waitForElement(this.appId_, itemDenyDrop);
    await remoteCall.waitForElementLost(this.appId_, itemAcceptDrop);
  }

  /**
   * Drag files specified by `sourceQuery` to the target tree item specified by
   * the `targetLabel`.
   *
   * @param sourceQuery Query to specify the source element.
   * @param targetLabel The drop target tree item label.
   * @param skipDrop Set true to drag over (hover) the target only, and not send
   *    target drop or source dragend events.
   */
  async dragFilesToItemByLabel(
      sourceQuery: string, targetLabel: string, skipDrop: boolean):
      Promise<((dragEndQuery: string, dragLeave: boolean) => Promise<void>)> {
    const target = this.selectors_.itemByLabel(targetLabel);
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil(
            'fakeDragAndDrop', this.appId_, [sourceQuery, target, skipDrop]),
        'fakeDragAndDrop failed');
    // A function is being returned to let the caller finish drop if drop
    // is skipped above.
    if (skipDrop) {
      return this.finishDrop_.bind(this, target);
    }
    return async () => {};
  }

  /**
   * @param targetQuery Query to specify the drop target.
   * @param dragEndQuery Query to specify which element to trigger the dragend
   *     event.
   * @param dragLeave Set true to send a dragleave event to the target instead
   *    of a drop event.
   */
  private async finishDrop_(
      targetQuery: string, dragEndQuery: string,
      dragLeave: boolean): Promise<void> {
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil(
            'fakeDragLeaveOrDrop', this.appId_,
            [dragEndQuery, targetQuery, dragLeave]),
        'fakeDragLeaveOrDrop failed');
  }

  /**
   * Use keyboard shortcut to trigger rename for a tree item.
   *
   * @param label Label of the tree item to trigger rename.
   */
  async triggerRenameWithKeyboardByLabel(label: string): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label, {focused: true});

    // Press rename <Ctrl>-Enter keyboard shortcut on the tree item.
    const renameKey = [
      itemSelector,
      'Enter',
      true,
      false,
      false,
    ];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', this.appId_, renameKey);
  }

  /**
   * Waits for the rename input to show inside the tree item.
   *
   * @param label Label of the tree item.
   */
  async waitForRenameInputByLabel(label: string): Promise<ElementObject> {
    const itemSelector = this.selectors_.itemByLabel(label);
    const textInput = this.selectors_.renameInput(itemSelector);
    return remoteCall.waitForElement(this.appId_, textInput);
  }

  /**
   * Input the new name to the tree item specified by its label without pressing
   * Enter to commit.
   *
   * @param label Label of the tree item.
   * @param newName The new name.
   */
  async inputNewNameForItemByLabel(label: string, newName: string):
      Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label);
    // Check: the renaming text input element should appear.
    const textInputSelector = this.selectors_.renameInput(itemSelector);
    await remoteCall.waitForElement(this.appId_, textInputSelector);

    // Enter the new name for the tree item.
    await remoteCall.inputText(this.appId_, textInputSelector, newName);
  }

  /**
   * Renames the tree item specified by the label to the new name.
   *
   * @param label Label of the tree item.
   * @param newName The new name.
   */
  async renameItemByLabel(label: string, newName: string): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label);
    const textInputSelector = this.selectors_.renameInput(itemSelector);
    await this.inputNewNameForItemByLabel(label, newName);

    // Press Enter key to end text input.
    const enterKey = [textInputSelector, 'Enter', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', this.appId_, enterKey);

    // Wait for the renaming input element to disappear.
    await remoteCall.waitForElementLost(this.appId_, textInputSelector);

    // Wait until renaming is complete.
    const renamingItemSelector = this.selectors_.attachModifier(
        `${this.selectors_.root} ${this.selectors_.item}`, {renaming: true});
    await remoteCall.waitForElementLost(this.appId_, renamingItemSelector);
  }

  /**
   * Wait for the tree item specified by label to finish drag/drop.
   *
   * @param label Label of the tree item.
   */
  async waitForItemToFinishDropByLabel(label: string): Promise<void> {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await remoteCall.waitForElementLost(this.appId_, itemDenyDrop);
    await remoteCall.waitForElementLost(this.appId_, itemAcceptDrop);
  }

  /**
   * Select the tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async selectItemByLabel(label: string): Promise<void> {
    await this.selectItem_(this.selectors_.itemByLabel(label));
  }

  /**
   * Select the tree item by its type.
   *
   * @param type Type of the tree item.
   */
  async selectItemByType(type: string): Promise<void> {
    if (this.selectors_.isInsideDrive(type)) {
      await this.expandTreeItemByLabel('Google Drive');
    }
    await this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Select the tree item by its path.
   *
   * @param path Full path of the tree item.
   */
  async selectItemByPath(path: string): Promise<void> {
    await this.selectItem_(this.selectors_.itemByPath(path));
  }

  /**
   * Select the group root tree item (e.g. entry list) by its type.
   *
   * @param type Type of the tree item.
   */
  async selectGroupRootItemByType(type: string): Promise<void> {
    await this.selectItem_(this.selectors_.groupRootItemByType(type));
  }

  /**
   * Select the placeholder tree item by its type.
   *
   * @param type Type of the placeholder tree item.
   */
  async selectPlaceholderItemByType(type: string): Promise<void> {
    await this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Select the shortcut tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async selectShortcutItemByLabel(label: string): Promise<void> {
    await this.selectItem_(
        this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Show context menu for the tree item by its label.
   *
   * @param label Label of the tree item.
   */
  async showContextMenuForItemByLabel(label: string): Promise<void> {
    await this.showItemContextMenu_(this.selectors_.itemByLabel(label));
  }

  /**
   * Long press the tree item by its label to trigger context menu.
   *
   * @param label Label of the tree item.
   */
  async longPressItemByLabel(label: string): Promise<void> {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeContextMenu', this.appId_,
            [this.selectors_.itemByLabel(label)]),
        'fakeContextMenu failed');
  }

  /**
   * Show context menu for the tree item by its full path.
   *
   * @param path Path of the tree item.
   */
  async showContextMenuForItemByPath(path: string): Promise<void> {
    await this.showItemContextMenu_(this.selectors_.itemByPath(path));
  }

  /**
   * Show context menu for the shortcut item by its label.
   *
   * @param label Label of the shortcut tree item.
   */
  async showContextMenuForShortcutItemByLabel(label: string): Promise<void> {
    await this.showItemContextMenu_(
        this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Show context menu for the eject button inside the tree item.
   *
   * @param label Label of the tree item.
   */
  async showContextMenuForEjectButtonByLabel(label: string): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label);
    const ejectButton = this.selectors_.ejectButton(itemSelector);
    await remoteCall.waitForElement(this.appId_, ejectButton);
    // Focus on the eject button.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'focus', this.appId_, [ejectButton]),
        'focus failed: eject button');

    // Right click the eject button.
    await remoteCall.waitAndRightClick(this.appId_, ejectButton);
  }

  /**
   * Show context menu for the rename input inside the tree item.
   *
   * @param label Label of the tree item.
   */
  async showContextMenuForRenameInputByLabel(label: string): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label);
    const renameInput = this.selectors_.renameInput(itemSelector);
    await remoteCall.waitAndRightClick(this.appId_, renameInput);
  }

  /**
   * Focus the tree.
   *
   */
  async focusTree(): Promise<void> {
    await remoteCall.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);
  }

  /**
   * Send a blur even to the tree item specified by its label.
   *
   * @param label Label of the tree item.
   */
  async blurItemByLabel(label: string): Promise<void> {
    const itemSelector = this.selectors_.itemByLabel(label);
    const iconSelector = [
      itemSelector,
      '.tree-item > .tree-row-wrapper > .tree-row > .tree-label-icon',
    ];

    await remoteCall.callRemoteTestUtil(
        'fakeEvent', this.appId_, [iconSelector, 'blur']);
  }

  /** Show the context menu for a tree item by right clicking it. */
  private async showItemContextMenu_(itemSelector: string): Promise<void> {
    await remoteCall.waitAndRightClick(this.appId_, itemSelector);
  }

  /** Select the tree item by clicking it. */
  private async selectItem_(itemSelector: string): Promise<void> {
    await remoteCall.waitAndClickElement(this.appId_, [itemSelector]);
  }

  /**
   * Expands a single tree item by clicking on its expand icon.
   *
   * @param itemSelector Selector to the tree item that should be expanded.
   * @param allowEmpty Allow expanding tree item without any children.
   */
  private async expandTreeItem_(itemSelector: string, allowEmpty?: boolean):
      Promise<void> {
    await remoteCall.waitForElement(this.appId_, itemSelector);
    const elements = await remoteCall.callRemoteTestUtil<ElementObject[]>(
        'queryAllElements', this.appId_,
        [this.selectors_.attachModifier(itemSelector, {expanded: true})]);
    // If it's already expanded just set the focus on directory tree.
    if (elements.length > 0) {
      return;
    }

    // Use array here because they are inside shadow DOM.
    const expandIcon = [
      this.selectors_.attachModifier(itemSelector, {expanded: false}),
      '.tree-item > .tree-row-wrapper > .tree-row > .expand-icon',
    ];
    const expandedSubtree = [
      this.selectors_.attachModifier(itemSelector, {expanded: true}),
      '.tree-item[aria-expanded="true"]',
    ];


    await remoteCall.waitAndClickElement(this.appId_, expandIcon);
    if (!allowEmpty) {
      // Wait for the expansion to finish.
      await remoteCall.waitForElement(this.appId_, expandedSubtree);
    }
  }

  /**
   * Collapses a single tree item by clicking on its expand icon.
   *
   * @param itemSelector Selector to the tree item that should be expanded.
   */
  private async collapseTreeItem_(itemSelector: string): Promise<void> {
    await remoteCall.waitForElement(this.appId_, itemSelector);
    const elements = await remoteCall.callRemoteTestUtil<ElementObject[]>(
        'queryAllElements', this.appId_,
        [this.selectors_.attachModifier(itemSelector, {expanded: false})]);
    // If it's already collapsed just set the focus on directory tree.
    if (elements.length > 0) {
      return;
    }

    // Use array here because they are inside shadow DOM.
    const expandIcon = [
      this.selectors_.attachModifier(itemSelector, {expanded: true}),
      '.tree-item > .tree-row-wrapper > .tree-row > .expand-icon',
    ];

    await remoteCall.waitAndClickElement(this.appId_, expandIcon);
    await remoteCall.waitForElement(
        this.appId_,
        this.selectors_.attachModifier(itemSelector, {expanded: false}));
  }
}

/**
 * Selectors of DirectoryTree, all the method provided by this class return the
 * selector string.
 */
class DirectoryTreeSelectors {
  /** The root selector of the directory tree. */
  get root(): string {
    return '#directory-tree';
  }

  /** The container selector of the directory tree. */
  get container(): string {
    return '.dialog-navigation-list';
  }

  /** The tree item selector. */
  get item(): string {
    return 'xf-tree-item';
  }

  /** Get tree item by the label of the item. */
  itemByLabel(label: string, modifiers?: ModifierOptions): string {
    const itemSelector = `${this.root} ${this.itemItselfByLabel(label)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /** Get tree item by the full path of the item. */
  itemByPath(path: string, modifiers?: ModifierOptions): string {
    const itemSelector = `${this.root} ${this.itemItselfByPath(path)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /** Get tree item by the type of the item. */
  itemByType(
      type: string, isPlaceholder?: boolean,
      modifiers?: ModifierOptions): string {
    const itemSelector =
        `${this.root} ${this.itemItselfByType(type, !!isPlaceholder)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /** Get the group root tree item (e.g. entry list) by the type of the item. */
  groupRootItemByType(type: string, modifiers?: ModifierOptions): string {
    const itemSelector = `${this.root} ${this.groupRootItemItselfByType(type)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /** Get all expanded tree items. */
  expandedItems(): string {
    return `${this.root} ${this.attachModifier(this.item, {expanded: true})}`;
  }

  /** Get all the direct child items of the specific item. */
  childItems(parentSelector: string): string {
    return `${parentSelector} > ${this.item}`;
  }

  /**
   * Get the direct child item under a specific parent item.
   *
   * @param parentSelector The parent item selector.
   * @param childSelector The child item selector.
   */
  childItem(parentSelector: string, childSelector: string): string {
    return `${parentSelector} ${childSelector}`;
  }

  /**
   * Get all direct child items of the specific item, which are not empty (have
   * nested children inside).
   */
  nonEmptyChildItems(itemSelector: string): string {
    // For new tree implementation, `hasChildren` will only be true when there's
    // actual tree item rendered inside, hence the use of `mayHaveChildren`
    // here instead of `hasChildren`.
    return this.attachModifier(
        `${itemSelector} > ${this.item}`, {mayHaveChildren: true});
  }

  /** Get the eject button of the specific tree item. */
  ejectButton(itemSelector: string): string {
    return `${itemSelector} .root-eject`;
  }

  /** Get the expand icon of the specific tree item. */
  expandIcon(itemSelector: string): string|string[] {
    // Use array here because they are inside shadow DOM.
    return [itemSelector, '.expand-icon'];
  }

  /** Get the rename input of the specific tree item. */
  renameInput(itemSelector: string): string {
    return `${itemSelector} > input`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its type.
   *
   * @param isPlaceholder Is the tree item a placeholder or not.
   */
  itemItselfByType(type: string, isPlaceholder: boolean): string {
    // volume type for "My files" is "downloads", but in the code when we
    // query item by "downloads" type, what we want is the actual Downloads
    // folder, hence the special handling here.
    if (type === 'downloads') {
      return `${this.item}[data-navigation-key^="${
          REAL_ENTRY_PATH_PREFIX}"][icon="downloads"]`;
    }

    return isPlaceholder ?
        `${this.item}[data-navigation-key^="${FAKE_ENTRY_PATH_PREFIX}"][icon="${
            type}"]` :
        `${this.item}[data-navigation-key^="${
            REAL_ENTRY_PATH_PREFIX}"][volume-type-for-testing="${type}"]`;
  }

  /**
   * Get the group root tree item (e.g. entry list) itself (without the parent
   * tree selector) by its type.
   */
  groupRootItemItselfByType(type: string): string {
    // For EntryList, there are some differences between the old/new tree on the
    // icon names. Format: <old-tree-icon-name>: <new-tree-icon-name>.
    const iconNameMap: Record<string, string> = {
      'drive': 'service_drive',
      'removable': 'usb',
    };
    if (type in iconNameMap) {
      type = iconNameMap[type]!;
    }
    return `${this.item}[data-navigation-key^="${
        ENTRY_LIST_PATH_PREFIX}"][icon="${type}"]`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its label.
   *
   * @param label The label of the tree item.
   */
  itemItselfByLabel(label: string): string {
    return `${this.item}[label="${label}"]`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its path.
   *
   * @param path The full path of the tree item.
   */
  itemItselfByPath(path: string): string {
    return `${this.item}[full-path-for-testing="${path}"]`;
  }

  /**
   * Check if the volume type is inside the Google Drive volume or not.
   *
   * @param type The volume type of the tree item.
   */
  isInsideDrive(type: string): boolean {
    return type === 'drive_recent' || type === 'drive_shared_with_me' ||
        type === 'drive_offline' || type === 'shared_drive' ||
        type === 'computer';
  }

  /** Return the recipient element of the keyboard event. */
  get keyboardRecipient() {
    return this.root;
  }

  /** Append the modifier selector to the item selector. */
  attachModifier(itemSelector: string, modifiers: ModifierOptions = {}) {
    const appendedSelectors: string[] = [];
    if (typeof modifiers.expanded !== 'undefined') {
      appendedSelectors.push(
          modifiers.expanded ? '[expanded]' : ':not([expanded])');
    }
    if (modifiers.selected) {
      appendedSelectors.push('[selected]');
    }
    if (modifiers.renaming) {
      appendedSelectors.push('[renaming]');
    }
    if (typeof modifiers.acceptDrop !== 'undefined') {
      appendedSelectors.push(modifiers.acceptDrop ? '.accepts' : '.denies');
    }
    if (typeof modifiers.hasChildren !== 'undefined') {
      appendedSelectors.push(
          `[has-children="${String(modifiers.hasChildren)}"]`);
    }
    if (modifiers.mayHaveChildren) {
      appendedSelectors.push('[may-have-children]');
    }
    if (modifiers.currentDirectory) {
      appendedSelectors.push('[aria-description="Current directory"]');
    }
    if (modifiers.shortcut) {
      appendedSelectors.push('[icon="shortcut"]');
    }
    // ":focus" is a pseudo-class selector, should be put at the end.
    if (modifiers.focused) {
      appendedSelectors.push(':focus');
    }
    return `${itemSelector}${appendedSelectors.join('')}`;
  }
}

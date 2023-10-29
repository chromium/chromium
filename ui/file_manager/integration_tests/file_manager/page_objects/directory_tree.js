// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ElementObject} from '../../element_object.js';
import {RemoteCallFilesApp} from '../../remote_call.js';
import {sendTestMessage} from '../../test_util.js';


const FAKE_ENTRY_PATH_PREFIX = 'fake-entry:';
const ENTRY_LIST_PATH_PREFIX = 'entry-list:';
const REAL_ENTRY_PATH_PREFIX = 'filesystem:chrome://file-manager/external';

/**
 * This serves as the additional selector of the tree item.
 *
 * @typedef {{
 *   expanded: (?boolean|undefined),
 *   selected: (?boolean|undefined),
 *   focused: (?boolean|undefined),
 *   shortcut: (?boolean|undefined),
 *   renaming: (?boolean|undefined),
 *   acceptDrop: (?boolean|undefined),
 *   hasChildren: (?boolean|undefined),
 *   mayHaveChildren: (?boolean|undefined),
 *   currentDirectory: (?boolean|undefined),
 * }}
 */
let ModifierOptions;

/**
 * Page object for Directory Tree, this class abstracts all the selectors
 * related to directory tree and its tree items.
 */
export class DirectoryTreePageObject {
  /**
   * Return a singleton instance of DirectoryTreePageObject. This will make sure
   * the directory tree DOM element is ready.
   *
   * @param {string} appId
   * @param {!RemoteCallFilesApp} remoteCall
   * @return {!Promise<DirectoryTreePageObject>}
   */
  static async create(appId, remoteCall) {
    const useNewTree =
        await sendTestMessage({name: 'isNewDirectoryTreeEnabled'}) === 'true';
    const directoryTree =
        new DirectoryTreePageObject(appId, remoteCall, useNewTree);
    remoteCall.waitForElement(appId, directoryTree.rootSelector);
    return directoryTree;
  }

  /**
   * Note: do not use constructor directly, use `create` static method
   * instead, which will fetch the `useNewTree_` value and make sure the tree
   * DOM element is ready.
   *
   * @param {string} appId
   * @param {!RemoteCallFilesApp} remoteCall
   * @param {boolean} useNewTree
   */
  constructor(appId, remoteCall, useNewTree) {
    /** @private {string} */
    this.appId_ = appId;
    /** @private {!RemoteCallFilesApp} */
    this.remoteCall_ = remoteCall;
    /** @private {boolean} */
    this.useNewTree_ = useNewTree;
    /** @private {!DirectoryTreeSelectors_} */
    this.selectors_ = new DirectoryTreeSelectors_(useNewTree);
  }

  /**
   * Returns the selector for the tree root.
   * @return {string}
   */
  get rootSelector() {
    return this.selectors_.root;
  }

  /**
   * Returns the selector for the tree container.
   * @return {string}
   */
  get containerSelector() {
    return this.selectors_.container;
  }

  /**
   * Wait for the selected(aka "active" in the old tree implementation) tree
   * item with the label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForSelectedItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {selected: true}));
  }

  /**
   * Wait for the tree item with the label to have focused (aka "selected" in
   * the old tree implementation) state.
   *
   * @param {string} label Label of the tree item
   * @return {!Promise<!ElementObject>}
   */
  async waitForFocusedItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {focused: true}));
  }

  /**
   * Wait for the tree item with the type to have focused (aka "selected" in the
   * old tree implementation) state.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForFocusedItemByType(type) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByType(
            type, /* isPlaceholder= */ false, {focused: true}));
  }

  /**
   * Wait for the tree item with the label to have the the current directory
   * aria-description attribute.
   *
   * @param {string} label Label of the tree item
   * @return {!Promise<!ElementObject>}
   */
  async waitForCurrentDirectoryItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {currentDirectory: true}));
  }

  /**
   * Wait for the child items of the specific parent item to match the count.
   *
   * @param {string} parentLabel Label of the parent tree item.
   * @param {number} count Expected number of the child items.
   * @param {boolean=} excludeEmptyChild Set true to only return child items
   *     with nested children.
   * @return {!Promise}
   */
  async waitForChildItemsCountByLabel(parentLabel, count, excludeEmptyChild) {
    const itemSelector = this.selectors_.itemByLabel(parentLabel);
    const childItemsSelector = excludeEmptyChild ?
        this.selectors_.nonEmptyChildItems(itemSelector) :
        this.selectors_.childItems(itemSelector);
    return this.remoteCall_.waitForElementsCount(
        this.appId_, [childItemsSelector], count);
  }

  /**
   * Wait for the placeholder tree items specified by type to match the count.
   *
   * @param {string} type Type of the placeholder tree item.
   * @param {number} count Expected number of the child items.
   * @return {!Promise}
   */
  async waitForPlaceholderItemsCountByType(type, count) {
    const itemSelector =
        this.selectors_.itemByType(type, /* isPlaceholder= */ true);
    return this.remoteCall_.waitForElementsCount(
        this.appId_, [itemSelector], count);
  }

  /**
   * Get the currently focused tree item.
   *
   * @return {!Promise<?ElementObject>}
   */
  async getFocusedItem() {
    const focusedItemSelector = this.selectors_.attachModifier(
        `${this.selectors_.root} ${this.selectors_.item}`, {focused: true});
    const elements = await this.remoteCall_.callRemoteTestUtil(
        'deepQueryAllElements', this.appId_, [focusedItemSelector]);
    if (elements && elements.length > 0) {
      return elements[0];
    }
    return null;
  }

  /**
   * Get the label of the tree item.
   *
   * @param {?ElementObject} item The tree item.
   * @returns {string}
   */
  getItemLabel(item) {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
      return '';
    }
    return this.useNewTree_ ? item.attributes['label'] :
                              item.attributes['entry-label'];
  }

  /**
   * Get the volume type of the tree item.
   *
   * @param {?ElementObject} item The tree item.
   * @returns {string}
   */
  getItemVolumeType(item) {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
      return '';
    }
    return item.attributes['volume-type-for-testing'];
  }

  /**
   * Check if the tree item is disabled or not.
   *
   * @param {?ElementObject} item The tree item.
   */
  assertItemDisabled(item) {
    if (!item) {
      chrome.test.fail('Item is not a valid tree item.');
      return;
    }
    // Empty value for "disabled" means it's disabled.
    chrome.test.assertEq('', item.attributes['disabled']);
  }

  /**
   * Wait for the item with the label to get the `has-children` attribute with
   * the specified value.
   *
   * @param {string} label Label of the tree item.
   * @param {boolean} hasChildren should the tree item have children or not.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemToHaveChildrenByLabel(label, hasChildren) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {hasChildren: hasChildren}));
  }

  /**
   * Wait for the item with the type to get the `has-children` attribute with
   * the specified value.
   *
   * @param {string} type Type of the tree item.
   * @param {boolean} hasChildren should the tree item have children or not.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemToHaveChildrenByType(type, hasChildren) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByType(
            type, /* isPlaceholder= */ false, {hasChildren: hasChildren}));
  }

  /**
   * Wait for the item with the label to get the `may-have-children` attribute.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemToMayHaveChildrenByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {mayHaveChildren: true}));
  }

  /**
   * Wait for the item with the label to be expanded.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemToExpandByLabel(label) {
    const expandedItemSelector =
        this.selectors_.itemByLabel(label, {expanded: true});
    await this.remoteCall_.waitForElement(this.appId_, expandedItemSelector);
  }

  /**
   * Wait for the item with the label to be collapsed.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemToCollapseByLabel(label) {
    const collapsedItemSelector =
        this.selectors_.itemByLabel(label, {expanded: false});
    await this.remoteCall_.waitForElement(this.appId_, collapsedItemSelector);
  }

  /**
   * Expands a single tree item with the specified label by clicking on its
   * expand icon.
   * @param {string} label Label of the tree item we want to expand on.
   * @param {boolean=} allowEmpty Allow expanding tree item without
   *     any children.
   * @return {!Promise<void>}
   */
  async expandTreeItemByLabel(label, allowEmpty) {
    await this.expandTreeItem_(this.selectors_.itemByLabel(label), allowEmpty);
  }

  /**
   * Expands a single tree item with the specified full path by clicking on its
   * expand icon.
   * @param {string} path Path of the tree item we want to expand on.
   * @return {!Promise<void>}
   */
  async expandTreeItemByPath(path) {
    await this.expandTreeItem_(this.selectors_.itemByPath(path));
  }

  /**
   * Collapses a single tree item with the specified label by clicking on its
   * expand icon.
   * @param {string} label Label of the tree item we want to collapse on.
   * @return {!Promise<void>}
   */
  async collapseTreeItemByLabel(label) {
    await this.collapseTreeItem_(this.selectors_.itemByLabel(label));
  }

  /**
   * Expands each directory in the breadcrumbs path.
   *
   * @param {string} breadcrumbsPath Path based in the entry labels like:
   *     /My files/Downloads/photos.
   * @return {!Promise<string>} Promise fulfilled on success with the selector
   *    query of the last directory expanded.
   */
  async recursiveExpand(breadcrumbsPath) {
    const paths = breadcrumbsPath.split('/').filter(path => path);

    // Expand each directory in the breadcrumb.
    let query = this.selectors_.root;
    for (const parentLabel of paths) {
      // Wait for parent element to be displayed.
      query += ` ${this.selectors_.itemItselfByLabel(parentLabel)}`;
      await this.remoteCall_.waitForElement(this.appId_, query);

      // Only expand if element isn't expanded yet.
      const elements = await this.remoteCall_.callRemoteTestUtil(
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
   * @param {string} breadcrumbsPath Path based on the entry labels like:
   *     /My files/Downloads/photos to item that should navigate to.
   * @param {string=} shortcutToPath For shortcuts it navigates to a different
   *   breadcrumbs path, like /My Drive/ShortcutName.
   * @return {!Promise<string>} the final selector used to click on the
   * desired tree item.
   */
  async navigateToPath(breadcrumbsPath, shortcutToPath) {
    // Focus the directory tree.
    await this.focusTree();

    const paths = breadcrumbsPath.split('/');
    // For "/My Drive", expand the "Google Drive" first.
    if (this.useNewTree_ && paths[1] === 'My Drive') {
      paths.unshift('', 'Google Drive');
    }
    const leaf = paths.pop();

    // Expand all parents of the leaf entry.
    let query = await this.recursiveExpand(paths.join('/'));

    // Navigate to the final entry.
    query += ` ${this.selectors_.itemItselfByLabel(leaf)}`;
    await this.remoteCall_.waitAndClickElement(this.appId_, query);

    // Wait directory to finish scanning its content.
    await this.remoteCall_.waitForElement(
        this.appId_, `[scan-completed="${leaf}"]`);

    // If the search was not closed, wait for it to close.
    await this.remoteCall_.waitForElement(
        this.appId_, '#search-wrapper[collapsed]');

    // Wait to navigation to final entry to finish.
    await this.remoteCall_.waitUntilCurrentDirectoryIsChanged(
        this.appId_, (shortcutToPath || breadcrumbsPath));

    // Focus the directory tree.
    await this.focusTree();

    return query;
  }

  /**
   * Trigger a keydown event with ArrowUp key to move the focus to the previous
   * tree item.
   *
   * @return {!Promise<void>}
   */
  async focusPreviousItem() {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const arrowUp =
        [this.selectors_.keyboardRecipient, 'ArrowUp', false, false, false];
    await this.remoteCall_.callRemoteTestUtil(
        'fakeKeyDown', this.appId_, arrowUp);
  }

  /**
   * Trigger a keydown event with ArrowDown key to move the focus to the next
   * tree item.
   *
   * @return {!Promise<void>}
   */
  async focusNextItem() {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const arrowUp =
        [this.selectors_.keyboardRecipient, 'ArrowDown', false, false, false];
    await this.remoteCall_.callRemoteTestUtil(
        'fakeKeyDown', this.appId_, arrowUp);
  }

  /**
   * Trigger a keydown event with Enter key to select currently focused item.
   *
   * @return {!Promise<void>}
   */
  async selectFocusedItem() {
    // Focus the tree first before keyboard event.
    await this.focusTree();

    const enter =
        [this.selectors_.keyboardRecipient, 'Enter', false, false, false];
    await this.remoteCall_.callRemoteTestUtil(
        'fakeKeyDown', this.appId_, enter);
  }

  /**
   * Wait for the tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label));
  }

  /**
   * Wait for the tree item by its label to be lost.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemLostByLabel(label) {
    await this.remoteCall_.waitForElementLost(
        this.appId_, this.selectors_.itemByLabel(label));
  }

  /**
   * Wait for the tree item by its full path.
   *
   * @param {string} path Path of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemByPath(path) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByPath(path));
  }

  /**
   * Wait for the tree item by its full path to be lost.
   *
   * @param {string} path Path of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemLostByPath(path) {
    await this.remoteCall_.waitForElementLost(
        this.appId_, this.selectors_.itemByPath(path));
  }

  /**
   * Returns the labels for all visible tree items.
   *
   * @return {!Promise<!Array<string>>}
   */
  async getVisibleItemLabels() {
    const allItems =
        /** @type {!Array<!ElementObject>} */ (
            await this.remoteCall_.callRemoteTestUtil(
                'queryAllElements', this.appId_, [
                  `${this.selectors_.root} ${this.selectors_.item}`,
                  ['visibility'],
                ]));
    return allItems
        .filter(item => !item.hidden && item.styles['visibility'] !== 'hidden')
        .map(item => this.getItemLabel(item));
  }

  /**
   * Wait for the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemByType(type) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Wait for the tree item by its type to be lost.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemLostByType(type) {
    await this.remoteCall_.waitForElementLost(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Wait for the placeholder tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForPlaceholderItemByType(type) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Wait for the placeholder tree item by its type to be lost.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async waitForPlaceholderItemLostByType(type) {
    await this.remoteCall_.waitForElementLost(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Wait for the shortcut tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForShortcutItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Wait for the shortcut tree item by its label to be lost.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForShortcutItemLostByLabel(label) {
    await this.remoteCall_.waitForElementLost(
        this.appId_, this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Wait for the child tree item under a specified parent item by their label.
   *
   * @param {string} parentLabel Label of the parent item.
   * @param {string} childLabel Label of the child item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForChildItemByLabel(parentLabel, childLabel) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.childItem(
            this.selectors_.itemByLabel(parentLabel),
            this.selectors_.itemItselfByLabel(childLabel)));
  }

  /**
   * Wait for the child tree item to be lost under a specified parent item by
   * its label.
   *
   * @param {string} parentLabel Label of the parent item.
   * @param {string} childLabel Label of the child item.
   * @return {!Promise<void>}
   */
  async waitForChildItemLostByLabel(parentLabel, childLabel) {
    await this.remoteCall_.waitForElementLost(
        this.appId_,
        this.selectors_.childItem(
            this.selectors_.itemByLabel(parentLabel),
            this.selectors_.itemItselfByLabel(childLabel)));
  }

  /**
   * Wait for the group root tree item (e.g. entry list) by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForGroupRootItemByType(type) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.groupRootItemByType(type));
  }

  /**
   * Returns the child items of a parent item specified by its label.
   *
   * @param {string} parentLabel Label of the parent item.
   * @return {!Promise<!Array<!ElementObject>>}
   */
  async getChildItemsByParentLabel(parentLabel) {
    const parentItemSelector = this.selectors_.itemByLabel(parentLabel);
    const childItemsSelector = this.selectors_.childItems(parentItemSelector);
    return this.remoteCall_.callRemoteTestUtil(
        'queryAllElements', this.appId_, [childItemsSelector]);
  }

  /**
   * Wait for the eject button under the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemEjectButtonByType(type) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Wait for the eject button to be lost under the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemEjectButtonLostByType(type) {
    await this.remoteCall_.waitForElementLost(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Click the eject button under the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async ejectItemByType(type) {
    return this.remoteCall_.waitAndClickElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByType(type)));
  }

  /**
   * Click the eject button under the tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async ejectItemByLabel(label) {
    return this.remoteCall_.waitAndClickElement(
        this.appId_,
        this.selectors_.ejectButton(this.selectors_.itemByLabel(label)));
  }

  /**
   * Wait for the expand icon under the tree item to hide by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemExpandIconToHideByLabel(label) {
    const expandIcon =
        this.selectors_.expandIcon(this.selectors_.itemByLabel(label));
    const element = await this.remoteCall_.waitForElementStyles(
        this.appId_,
        expandIcon,
        ['visibility'],
    );
    chrome.test.assertEq('hidden', element.styles['visibility']);
  }

  /**
   * Wait for the tree item specified by label to accept drag/drop.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemToAcceptDropByLabel(label) {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await this.remoteCall_.waitForElement(this.appId_, itemAcceptDrop);
    await this.remoteCall_.waitForElementLost(this.appId_, itemDenyDrop);
  }

  /**
   * Wait for the tree item specified by label to deny drag/drop.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemToDenyDropByLabel(label) {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await this.remoteCall_.waitForElement(this.appId_, itemDenyDrop);
    await this.remoteCall_.waitForElementLost(this.appId_, itemAcceptDrop);
  }

  /**
   * Drag files specified by `sourceQuery` to the target tree item specified by
   * the `targetLabel`.
   *
   * @param {string} sourceQuery Query to specify the source element.
   * @param {string} targetLabel The drop target tree item label.
   * @param {boolean} skipDrop Set true to drag over (hover) the target
   *    only, and not send target drop or source dragend events.
   * @return {!Promise<!function(string, boolean):Promise<void>>}
   */
  async dragFilesToItemByLabel(sourceQuery, targetLabel, skipDrop) {
    const target = this.selectors_.itemByLabel(targetLabel);
    chrome.test.assertTrue(
        await this.remoteCall_.callRemoteTestUtil(
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
   *
   * @param {string} targetQuery Query to specify the drop target.
   * @param {string} dragEndQuery Query to specify which element to trigger
   *     the dragend event.
   * @param {boolean} dragLeave Set true to send a dragleave event to
   *    the target instead of a drop event.
   * @return {!Promise<void>}
   */
  async finishDrop_(targetQuery, dragEndQuery, dragLeave) {
    chrome.test.assertTrue(
        await this.remoteCall_.callRemoteTestUtil(
            'fakeDragLeaveOrDrop', this.appId_,
            [dragEndQuery, targetQuery, dragLeave]),
        'fakeDragLeaveOrDrop failed');
  }

  /**
   * Use keyboard shortcut to trigger rename for a tree item.
   *
   * @param {string} label Label of the tree item to trigger rename.
   * @return {!Promise<void>}
   */
  async triggerRenameWithKeyboardByLabel(label) {
    const itemSelector = this.selectors_.itemByLabel(label, {focused: true});

    // Press rename <Ctrl>-Enter keyboard shortcut on the tree item.
    const renameKey = [
      itemSelector,
      'Enter',
      true,
      false,
      false,
    ];
    await this.remoteCall_.callRemoteTestUtil(
        'fakeKeyDown', this.appId_, renameKey);
  }

  /**
   * Waits for the rename input to show inside the tree item.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForRenameInputByLabel(label) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const textInput = this.selectors_.renameInput(itemSelector);
    return this.remoteCall_.waitForElement(this.appId_, textInput);
  }

  /**
   * Input the new name to the tree item specified by its label without pressing
   * Enter to commit.
   *
   * @param {string} label Label of the tree item.
   * @param {string} newName The new name.
   * @return {!Promise<void>}
   */
  async inputNewNameForItemByLabel(label, newName) {
    const itemSelector = this.selectors_.itemByLabel(label);
    // Check: the renaming text input element should appear.
    const textInputSelector = this.selectors_.renameInput(itemSelector);
    await this.remoteCall_.waitForElement(this.appId_, textInputSelector);

    // Enter the new name for the tree item.
    await this.remoteCall_.inputText(this.appId_, textInputSelector, newName);
  }

  /**
   * Renames the tree item specified by the label to the new name.
   *
   * @param {string} label Label of the tree item.
   * @param {string} newName The new name.
   * @return {!Promise<void>}
   */
  async renameItemByLabel(label, newName) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const textInputSelector = this.selectors_.renameInput(itemSelector);
    await this.inputNewNameForItemByLabel(label, newName);

    // Press Enter key to end text input.
    const enterKey = [textInputSelector, 'Enter', false, false, false];
    await this.remoteCall_.callRemoteTestUtil(
        'fakeKeyDown', this.appId_, enterKey);

    // Wait for the renaming input element to disappear.
    await this.remoteCall_.waitForElementLost(this.appId_, textInputSelector);

    // Wait until renaming is complete.
    const renamingItemSelector = this.selectors_.attachModifier(
        `${this.selectors_.root} ${this.selectors_.item}`, {renaming: true});
    await this.remoteCall_.waitForElementLost(
        this.appId_, renamingItemSelector);
  }

  /**
   * Wait for the tree item specified by label to finish drag/drop.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async waitForItemToFinishDropByLabel(label) {
    const itemAcceptDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: true});
    const itemDenyDrop =
        this.selectors_.itemByLabel(label, {acceptDrop: false});
    await this.remoteCall_.waitForElementLost(this.appId_, itemDenyDrop);
    await this.remoteCall_.waitForElementLost(this.appId_, itemAcceptDrop);
  }

  /**
   * Select the tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async selectItemByLabel(label) {
    await this.selectItem_(this.selectors_.itemByLabel(label));
  }

  /**
   * Select the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async selectItemByType(type) {
    if (this.selectors_.isInsideDrive(type)) {
      await this.expandTreeItemByLabel('Google Drive');
    }
    await this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Select the tree item by its path.
   *
   * @param {string} path Full path of the tree item.
   * @return {!Promise<void>}
   */
  async selectItemByPath(path) {
    await this.selectItem_(this.selectors_.itemByPath(path));
  }

  /**
   * Select the group root tree item (e.g. entry list) by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async selectGroupRootItemByType(type) {
    await this.selectItem_(this.selectors_.groupRootItemByType(type));
  }

  /**
   * Select the placeholder tree item by its type.
   *
   * @param {string} type Type of the placeholder tree item.
   * @return {!Promise<void>}
   */
  async selectPlaceholderItemByType(type) {
    await this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Select the shortcut tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async selectShortcutItemByLabel(label) {
    await this.selectItem_(
        this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Show context menu for the tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async showContextMenuForItemByLabel(label) {
    await this.showItemContextMenu_(this.selectors_.itemByLabel(label));
  }

  /**
   * Show context menu for the tree item by its full path.
   *
   * @param {string} path Path of the tree item.
   * @return {!Promise<void>}
   */
  async showContextMenuForItemByPath(path) {
    await this.showItemContextMenu_(this.selectors_.itemByPath(path));
  }

  /**
   * Show context menu for the shortcut item by its label.
   *
   * @param {string} label Label of the shortcut tree item.
   * @return {!Promise<void>}
   */
  async showContextMenuForShortcutItemByLabel(label) {
    await this.showItemContextMenu_(
        this.selectors_.itemByLabel(label, {shortcut: true}));
  }

  /**
   * Show context menu for the eject button inside the tree item.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async showContextMenuForEjectButtonByLabel(label) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const ejectButton = this.selectors_.ejectButton(itemSelector);
    await this.remoteCall_.waitForElement(this.appId_, ejectButton);
    // Focus on the eject button.
    chrome.test.assertTrue(
        !!await this.remoteCall_.callRemoteTestUtil(
            'focus', this.appId_, [ejectButton]),
        'focus failed: eject button');

    // Right click the eject button.
    await this.remoteCall_.waitAndRightClick(this.appId_, ejectButton);
  }

  /**
   * Show context menu for the rename input inside the tree item.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async showContextMenuForRenameInputByLabel(label) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const renameInput = this.selectors_.renameInput(itemSelector);
    await this.remoteCall_.waitAndRightClick(this.appId_, renameInput);
  }

  /**
   * Focus the tree.
   *
   * @return {!Promise<void>}
   */
  async focusTree() {
    await this.remoteCall_.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);
  }

  /**
   * Send a blur even to the tree item specified by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<void>}
   */
  async blurItemByLabel(label) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const iconSelector = this.useNewTree_ ?
        [
          itemSelector,
          '.tree-item > .tree-row-wrapper > .tree-row > .tree-label-icon',
        ] :
        `${itemSelector} > .tree-row .item-icon`;
    await this.remoteCall_.callRemoteTestUtil(
        'fakeEvent', this.appId_, [iconSelector, 'blur']);
  }

  /**
   * Show the context menu for a tree item by right clicking it.
   *
   * @private
   * @param {string} itemSelector
   * @return {!Promise<void>}
   */
  async showItemContextMenu_(itemSelector) {
    await this.remoteCall_.waitAndRightClick(this.appId_, itemSelector);
  }

  /**
   * Select the tree item by clicking it.
   *
   * @private
   * @param {string} itemSelector
   * @return {!Promise<void>}
   */
  async selectItem_(itemSelector) {
    await this.remoteCall_.waitAndClickElement(this.appId_, [itemSelector]);
  }

  /**
   * Expands a single tree item by clicking on its expand icon.
   *
   * @private
   * @param {string} itemSelector Selector to the tree item that should be
   *     expanded.
   * @param {boolean=} allowEmpty Allow expanding tree item without
   *     any children.
   * @return {!Promise<void>}
   */
  async expandTreeItem_(itemSelector, allowEmpty) {
    await this.remoteCall_.waitForElement(this.appId_, itemSelector);
    const elements = await this.remoteCall_.callRemoteTestUtil(
        'queryAllElements', this.appId_,
        [this.selectors_.attachModifier(itemSelector, {expanded: true})]);
    // If it's already expanded just set the focus on directory tree.
    if (elements.length > 0) {
      if (!this.useNewTree_) {
        return this.focusTree();
      }
      return;
    }

    let expandIcon;
    let expandedSubtree;
    if (this.useNewTree_) {
      // Use array here because they are inside shadow DOM.
      expandIcon = [
        this.selectors_.attachModifier(itemSelector, {expanded: false}),
        '.tree-item > .tree-row-wrapper > .tree-row > .expand-icon',
      ];
      expandedSubtree = [
        this.selectors_.attachModifier(itemSelector, {expanded: true}),
        '.tree-item[aria-expanded="true"]',
      ];
    } else {
      expandIcon = `${this.selectors_.attachModifier(itemSelector, {
        expanded: false,
      })} > .tree-row:is([has-children=true], [may-have-children]) .expand-icon`;
      expandedSubtree = this.selectors_.attachModifier(
          `${itemSelector} > .tree-children`, {expanded: true});
    }

    await this.remoteCall_.waitAndClickElement(this.appId_, expandIcon);
    if (!allowEmpty) {
      // Wait for the expansion to finish.
      await this.remoteCall_.waitForElement(this.appId_, expandedSubtree);
    }
    if (!this.useNewTree_) {
      // Force the focus on directory tree.
      await this.focusTree();
    }
  }

  /**
   * Collapses a single tree item by clicking on its expand icon.
   *
   * @private
   * @param {string} itemSelector Selector to the tree item that should be
   *     expanded.
   * @return {!Promise<void>}
   */
  async collapseTreeItem_(itemSelector) {
    await this.remoteCall_.waitForElement(this.appId_, itemSelector);
    const elements = await this.remoteCall_.callRemoteTestUtil(
        'queryAllElements', this.appId_,
        [this.selectors_.attachModifier(itemSelector, {expanded: false})]);
    // If it's already collapsed just set the focus on directory tree.
    if (elements.length > 0) {
      if (!this.useNewTree_) {
        return this.focusTree();
      }
      return;
    }

    let expandIcon;
    if (this.useNewTree_) {
      // Use array here because they are inside shadow DOM.
      expandIcon = [
        this.selectors_.attachModifier(itemSelector, {expanded: true}),
        '.tree-item > .tree-row-wrapper > .tree-row > .expand-icon',
      ];
    } else {
      expandIcon = `${this.selectors_.attachModifier(itemSelector, {
        expanded: true,
      })} > .tree-row:is([has-children=true], [may-have-children]) .expand-icon`;
    }

    await this.remoteCall_.waitAndClickElement(this.appId_, expandIcon);
    await this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.attachModifier(itemSelector, {expanded: false}));
    if (!this.useNewTree_) {
      // Force the focus on directory tree.
      await this.focusTree();
    }
  }
}

/**
 * Selectors of DirectoryTree, all the method provided by this class return
 * the selector string.
 */
class DirectoryTreeSelectors_ {
  /**
   * @param {boolean} useNewTree
   */
  constructor(useNewTree) {
    /** @type {boolean} */
    this.useNewTree = useNewTree;
  }

  /**
   * The root selector of the directory tree.
   *
   * @return {string}
   */
  get root() {
    return '#directory-tree';
  }

  /**
   * The container selector of the directory tree.
   *
   * @return {string}
   */
  get container() {
    return '.dialog-navigation-list';
  }

  /**
   * The tree item selector.
   *
   * @return {string}
   */
  get item() {
    return this.useNewTree ? 'xf-tree-item' : '.tree-item';
  }

  /**
   * Get tree item by the label of the item.
   *
   * @param {string} label
   * @param {ModifierOptions=} modifiers
   * @return {string}
   */
  itemByLabel(label, modifiers) {
    const itemSelector = `${this.root} ${this.itemItselfByLabel(label)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /**
   * Get tree item by the full path of the item.
   *
   * @param {string} path
   * @param {ModifierOptions=} modifiers
   * @return {string}
   */
  itemByPath(path, modifiers) {
    const itemSelector = `${this.root} ${this.itemItselfByPath(path)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /**
   * Get tree item by the type of the item.
   *
   * @param {string} type
   * @param {boolean=} isPlaceholder
   * @param {ModifierOptions=} modifiers
   * @return {string}
   */
  itemByType(type, isPlaceholder, modifiers) {
    const itemSelector =
        `${this.root} ${this.itemItselfByType(type, !!isPlaceholder)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /**
   * Get the group root tree item (e.g. entry list) by the type of the item.
   *
   * @param {string} type
   * @param {ModifierOptions=} modifiers
   * @return {string}
   */
  groupRootItemByType(type, modifiers) {
    const itemSelector = `${this.root} ${this.groupRootItemItselfByType(type)}`;
    return this.attachModifier(itemSelector, modifiers);
  }

  /**
   * Get all expanded tree items.
   *
   * @return {string}
   */
  expandedItems() {
    return `${this.root} ${this.attachModifier(this.item, {expanded: true})}`;
  }

  /**
   * Get all the direct child items of the specific item.
   *
   * @param {string} parentSelector
   * @return {string}
   */
  childItems(parentSelector) {
    return this.useNewTree ?
        `${parentSelector} > ${this.item}` :
        `${parentSelector} > .tree-children > ${this.item}`;
  }

  /**
   * Get the direct child item under a specific parent item.
   *
   * @param {string} parentSelector The parent item selector.
   * @param {string} childSelector The child item selector.
   * @return {string}
   */
  childItem(parentSelector, childSelector) {
    return this.useNewTree ?
        `${parentSelector} ${childSelector}` :
        `${parentSelector} .tree-children ${childSelector}`;
  }

  /**
   * Get all direct child items of the specific item, which are not empty (have
   * nested children inside).
   *
   * @param {string} itemSelector
   * @return {string}
   */
  nonEmptyChildItems(itemSelector) {
    const nestedItemSelector = this.useNewTree ?
        `${this.item}:has(${this.item})` :
        `.tree-children > ${this.item} > .tree-row`;
    return this.attachModifier(
        `${itemSelector} > ${nestedItemSelector}`, {hasChildren: true});
  }

  /**
   * Get the eject button of the specific tree item.
   *
   * @param {string} itemSelector
   * @return {string}
   */
  ejectButton(itemSelector) {
    return `${itemSelector} .root-eject`;
  }

  /**
   * Get the expand icon of the specific tree item.
   *
   * @param {string} itemSelector
   * @return {string|!Array<string>}
   */
  expandIcon(itemSelector) {
    // Use array here because they are inside shadow DOM.
    return this.useNewTree ? [itemSelector, '.expand-icon'] :
                             `${itemSelector} > .tree-row .expand-icon`;
  }

  /**
   * Get the rename input of the specific tree item.
   *
   * @param {string} itemSelector
   * @return {string}
   */
  renameInput(itemSelector) {
    return this.useNewTree ? `${itemSelector} > input` :
                             `${itemSelector} > .tree-row input`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its type.
   *
   * @param {string} type
   * @param {boolean} isPlaceholder Is the tree item a placeholder or not.
   * @return {string}
   */
  itemItselfByType(type, isPlaceholder) {
    if (this.useNewTree) {
      // volume type for "My files" is "downloads", but in the code when we
      // query item by "downloads" type, what we want is the actual Downloads
      // folder, hence the special handling here.
      if (type === 'downloads') {
        return `${this.item}[data-navigation-key^="${
            REAL_ENTRY_PATH_PREFIX}"][icon="downloads"]`;
      }

      return isPlaceholder ?
          `${this.item}[data-navigation-key^="${
              FAKE_ENTRY_PATH_PREFIX}"][icon="${type}"]` :
          `${this.item}[data-navigation-key^="${
              REAL_ENTRY_PATH_PREFIX}"][volume-type-for-testing="${type}"]`;
    }
    return isPlaceholder ?
        `${this.item}:has(> .tree-row [root-type-icon="${type}"])` :
        `${this.item}:has(> .tree-row [volume-type-icon="${type}"])`;
  }

  /**
   * Get the group root tree item (e.g. entry list) itself (without the parent
   * tree selector) by its type.
   *
   * @param {string} type
   * @return {string}
   */
  groupRootItemItselfByType(type) {
    // For EntryList, there are some differences between the old/new tree on the
    // icon names. Format: <old-tree-icon-name>: <new-tree-icon-name>.
    const iconNameMap = {
      'drive': 'service_drive',
      'removable': 'usb',
    };
    if (this.useNewTree && type in iconNameMap) {
      type = iconNameMap[type];
    }
    return this.useNewTree ?
        `${this.item}[data-navigation-key^="${ENTRY_LIST_PATH_PREFIX}"][icon="${
            type}"]` :
        `${this.item}:has(> .tree-row [root-type-icon="${type}"])`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its label.
   *
   * @param {string} label The label of the tree item.
   * @return {string}
   */
  itemItselfByLabel(label) {
    return this.useNewTree ? `${this.item}[label="${label}"]` :
                             `${this.item}[entry-label="${label}"]`;
  }

  /**
   * Get the tree item itself (without the parent tree selector) by its path.
   *
   * @param {string} path The full path of the tree item.
   * @return {string}
   */
  itemItselfByPath(path) {
    return `${this.item}[full-path-for-testing="${path}"]`;
  }

  /**
   * Check if the volume type is inside the Google Drive volume or not.
   *
   * @param {string} type The volume type of the tree item.
   * @return {boolean}
   */
  isInsideDrive(type) {
    return type == 'drive_recent' || type == 'drive_shared_with_me' ||
        type == 'drive_offline' || type == 'shared_drive' || type == 'computer';
  }

  /**
   * Return the recipient element of the keyboard event.
   */
  get keyboardRecipient() {
    return this.root;
  }

  /**
   * Append the modifier selector to the item selector.
   *
   * @param {string} itemSelector
   * @param {ModifierOptions=} modifiers
   */
  attachModifier(itemSelector, modifiers = {}) {
    const appendedSelectors = [];
    if (typeof modifiers.expanded !== 'undefined') {
      appendedSelectors.push(
          modifiers.expanded ? '[expanded]' : ':not([expanded])');
    }
    if (modifiers.selected) {
      appendedSelectors.push(
          this.useNewTree ? '[selected]' : ':has(.tree-row[active])');
    }
    if (modifiers.renaming) {
      appendedSelectors.push('[renaming]');
    }
    if (typeof modifiers.acceptDrop !== 'undefined') {
      appendedSelectors.push(modifiers.acceptDrop ? '.accepts' : '.denies');
    }
    if (typeof modifiers.hasChildren != 'undefined') {
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
      appendedSelectors.push(
          this.useNewTree ? '[icon="shortcut"]' : '[dir-type="ShortcutItem"]');
    }
    // ":focus" is a pseudo-class selector, should be put at the end.
    if (modifiers.focused) {
      appendedSelectors.push(this.useNewTree ? ':focus' : '[selected]');
    }
    return `${itemSelector}${appendedSelectors.join('')}`;
  }
}

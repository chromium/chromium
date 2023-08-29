// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ElementObject} from '../../element_object.js';
import {RemoteCallFilesApp} from '../../remote_call.js';
import {sendTestMessage} from '../../test_util.js';


const FAKE_ENTRY_PATH_PREFIX = 'fake-entry:';
const REAL_ENTRY_PATH_PREFIX = 'filesystem:chrome://file-manager/external';

/**
 * This serves as the additional selector of the tree item.
 *
 * @typedef {{
 *   expanded: (?boolean|undefined),
 *   selected: (?boolean|undefined),
 *   focused: (?boolean|undefined),
 *   renaming: (?boolean|undefined),
 *   acceptDrop: (?boolean|undefined),
 *   denyDrop: (?boolean|undefined),
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
        await sendTestMessage({name: 'isFilesExperimentalEnabled'}) === 'true';
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
   * @param {string} label Label of the tree item
   * @return {!Promise<!ElementObject>}
   */
  async waitForSelectedItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {selected: true}));
  }

  /**
   * Wait for the tree item with the label to have focused(aka "selected" in the
   * old tree implementation) state.
   *
   * @param {string} label Label of the tree item
   * @return {!Promise<!ElementObject>}
   */
  async waitForFocusedItemByLabel(label) {
    return this.remoteCall_.waitForElement(
        this.appId_, this.selectors_.itemByLabel(label, {focused: true}));
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
   * Wait for the child items of the specific item
   *
   * @param {string} label Label of the tree item
   * @param {number} count Expected number of the child items
   * @param {boolean=} excludeEmptyChild child items without any children
   * @return {Promise}
   */
  async waitForChildItemsCountByLabel(label, count, excludeEmptyChild) {
    const itemSelector = this.selectors_.itemByLabel(label);
    const childItemsSelector = excludeEmptyChild ?
        this.selectors_.childItemsWithNestedChildren(itemSelector) :
        this.selectors_.childItems(itemSelector);
    return this.remoteCall_.waitForElementsCount(
        this.appId_, [childItemsSelector], count);
  }

  /**
   * Get the label of the tree item.
   * @param {?ElementObject} item the tree item.
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
   * Wait for the item with the label to get the `has-children` attribute with
   * the specified value.
   *
   * @param {string} label the label of the tree item.
   * @param {boolean} hasChildren should hte tree item has children or not.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemToHaveChildren(label, hasChildren) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {hasChildren: hasChildren}));
  }

  /**
   * Wait for the item with the label to get the `may-have-children` attribute.
   *
   * @param {string} label the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForItemToMayHaveChildren(label) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByLabel(label, {mayHaveChildren: true}));
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
    this.expandTreeItem(this.selectors_.itemByLabel(label), allowEmpty);
  }

  /**
   * Expands each directory in the breadcrumbs path.
   *
   * @param {string} breadcrumbsPath Path based in the entry labels like:
   *     /My files/Downloads/photos
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
        await this.expandTreeItem(query);
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
    await this.remoteCall_.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);

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
    await this.remoteCall_.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);

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
    await this.remoteCall_.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);

    const arrowUp =
        [this.selectors_.keyboardRecipient, 'ArrowUp', false, false, false];
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
    await this.remoteCall_.callRemoteTestUtil(
        'focus', this.appId_, [this.selectors_.root]);

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
   * Wait for the placeholder tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async waitForPlaceholderItem(type) {
    return this.remoteCall_.waitForElement(
        this.appId_,
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
  }

  /**
   * Select the tree item by its label.
   *
   * @param {string} label Label of the tree item.
   * @return {!Promise<!ElementObject>}
   */
  async selectItemByLabel(label) {
    return this.remoteCall_.waitAndClickElement(
        this.appId_, this.selectors_.itemByLabel(label));
  }

  /**
   * Select the tree item by its type.
   *
   * @param {string} type Type of the tree item.
   * @return {!Promise<void>}
   */
  async selectItemByType(type) {
    this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ false));
  }

  /**
   * Select the placeholder tree item by its type.
   *
   * @param {string} type Type of the placeholder tree item.
   * @return {!Promise<void>}
   */
  async selectPlaceholderItem(type) {
    this.selectItem_(
        this.selectors_.itemByType(type, /* isPlaceholder= */ true));
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
   * TODO: this "selector" version should be private in future.
   *
   * @param {string} itemSelector Selector to the tree item that should be
   *     expanded.
   * @param {boolean=} allowEmpty Allow expanding tree item without
   *     any children.
   * @return {!Promise<void>}
   */
  async expandTreeItem(itemSelector, allowEmpty) {
    await this.remoteCall_.waitForElement(this.appId_, itemSelector);
    const elements = await this.remoteCall_.callRemoteTestUtil(
        'queryAllElements', this.appId_,
        [this.selectors_.attachModifier(itemSelector, {expanded: true})]);
    // If it's already expanded just set the focus on directory tree.
    if (elements.length > 0) {
      if (!this.useNewTree_) {
        return this.remoteCall_.callRemoteTestUtil(
            'focus', this.appId_, [this.selectors_.root]);
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
      await this.remoteCall_.callRemoteTestUtil(
          'focus', this.appId_, [this.selectors_.root]);
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
   * Get all the child items of the specific item.
   *
   * @param {string} itemSelector
   * @return {string}
   */
  childItems(itemSelector) {
    return `${itemSelector} > ${
        this.useNewTree ? 'xf-tree-item' : '.tree-children > .tree-item'}`;
  }

  /**
   * Get all the child items of the specific item, which have the nested
   * children.
   *
   * @param {string} itemSelector
   * @return {string}
   */
  childItemsWithNestedChildren(itemSelector) {
    const nestedItemSelector = this.useNewTree ?
        'xf-tree-item:has(xf-tree-item)' :
        '.tree-children > .tree-item > .tree-row';
    return this.attachModifier(
        `${itemSelector} > ${nestedItemSelector}`, {hasChildren: true});
  }

  /**
   *
   * @param {string} type
   * @param {boolean} isPlaceholder Is the tree item a placeholder or not.
   * @return {string}
   */
  itemItselfByType(type, isPlaceholder) {
    if (this.useNewTree) {
      return isPlaceholder ? `xf-tree-item[data-navigation-key^="${
                                 FAKE_ENTRY_PATH_PREFIX}"][icon="${type}"]` :
                             `xf-tree-item[data-navigation-key^="${
                                 REAL_ENTRY_PATH_PREFIX}"][icon="${type}"]`;
    }
    return isPlaceholder ? `[root-type-icon="${type}"]` :
                           `[volume-type-icon="${type}"]`;
  }

  /**
   *
   * @param {string} label The label of the tree item.
   * @return {string}
   */
  itemItselfByLabel(label) {
    return this.useNewTree ? `xf-tree-item[label="${label}"]` :
                             `.tree-item[entry-label="${label}"]`;
  }


  /**
   * Return the recipient element of the keyboard event.
   */
  get keyboardRecipient() {
    return this.useNewTree ? [this.root, 'ul'] : this.root;
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
    if (modifiers.acceptDrop) {
      appendedSelectors.push('.accepts');
    }
    if (modifiers.denyDrop) {
      appendedSelectors.push('.denies');
    }
    if (typeof modifiers.hasChildren != 'undefined') {
      appendedSelectors.push(`[has-children=${String(modifiers.hasChildren)}`);
    }
    if (modifiers.mayHaveChildren) {
      appendedSelectors.push('[may-have-children]');
    }
    if (modifiers.currentDirectory) {
      appendedSelectors.push('[aria-description="Current directory"]');
    }
    // ":focus" is a pseudo-class selector, should be put at the end.
    if (modifiers.focused) {
      appendedSelectors.push(this.useNewTree ? ':focus' : '[selected]');
    }
    return `${itemSelector}${appendedSelectors.join('')}`;
  }
}

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';

import {DialogType} from '../../../common/js/dialog_type.js';
import {queryRequiredElement} from '../../../common/js/dom_utils.js';
import {util} from '../../../common/js/util.js';
import {FileListModel, GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME} from '../file_list_model.js';
import {ListThumbnailLoader} from '../list_thumbnail_loader.js';

import {FileGrid} from './file_grid.js';
import {FileTable} from './file_table.js';
import {List} from './list.js';
import {ListItem} from './list_item.js';
import {ListSelectionModel} from './list_selection_model.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';

class TextSearchState {
  constructor() {
    /** @public {string} */
    this.text = '';

    /** @public {!Date} */
    this.date = new Date();
  }
}

/**
 * List container for the file table and the grid view.
 */
export class ListContainer {
  /**
   * @param {!HTMLElement} element Element of the container.
   * @param {!FileTable} table File table.
   * @param {!FileGrid} grid File grid.
   * @param {DialogType} type The type of the main dialog.
   */
  constructor(element, table, grid, type) {
    /**
     * The container element of the file list.
     * @type {!HTMLElement}
     * @const
     */
    this.element = element;

    /**
     * The file table.
     * @type {!FileTable}
     * @const
     */
    this.table = table;

    /**
     * The file grid.
     * @type {!FileGrid}
     * @const
     */
    this.grid = grid;

    /**
     * Current file list.
     * @type {ListContainer.ListType}
     */
    this.currentListType = ListContainer.ListType.UNINITIALIZED;

    /**
     * The input element to rename entry.
     * @type {!HTMLInputElement}
     * @const
     */
    this.renameInput =
        assertInstanceof(document.createElement('input'), HTMLInputElement);
    this.renameInput.className = 'rename entry-name';

    /**
     * Spinner on file list which is shown while loading.
     * @type {!HTMLElement}
     * @const
     */
    this.spinner =
        queryRequiredElement('files-spinner.loading-indicator', element);

    /**
     * @type {FileListModel}
     */
    this.dataModel = null;

    /**
     * @type {ListThumbnailLoader}
     */
    this.listThumbnailLoader = null;

    /**
     * @type {ListSelectionModel|ListSingleSelectionModel}
     */
    this.selectionModel = null;

    /**
     * Data model which is used as a placefolder in inactive file list.
     * @type {FileListModel}
     */
    this.emptyDataModel = null;

    /**
     * Selection model which is used as a placefolder in inactive file list.
     * @type {!ListSelectionModel}
     * @const
     * @private
     */
    this.emptySelectionModel_ = new ListSelectionModel();

    /**
     * @type {!TextSearchState}
     * @const
     */
    this.textSearchState = new TextSearchState();

    /**
     * Whtehter to allow or cancel a context menu event.
     * @type {boolean}
     * @private
     */
    this.allowContextMenuByTouch_ = false;

    /**
     * List container needs to know if the current active directory is Recent
     * or not so it can update groupBy filed accordingly.
     * @public {boolean}
     */
    this.isOnRecent = false;

    // Overriding the default role 'list' to 'listbox' for better accessibility
    // on ChromeOS.
    this.table.list.setAttribute('role', 'listbox');
    this.table.list.id = 'file-list';
    this.grid.setAttribute('role', 'listbox');
    this.grid.id = 'file-list';
    this.element.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.element.addEventListener('keypress', this.onKeyPress_.bind(this));
    this.element.addEventListener(
        'contextmenu', this.onContextMenu_.bind(this), /* useCapture */ true);

    // Disables context menu by long-tap when at least one file/folder is
    // selected, while still enabling two-finger tap.
    this.element.addEventListener('touchstart', function(e) {
      if (e.touches.length > 1) {
        this.allowContextMenuByTouch_ = true;
      }
    }.bind(this), {passive: true});
    this.element.addEventListener('touchend', function(e) {
      if (e.touches.length == 0) {
        // contextmenu event will be sent right after touchend.
        setTimeout(function() {
          this.allowContextMenuByTouch_ = false;
        }.bind(this));
      }
    }.bind(this));
    this.element.addEventListener('contextmenu', function(e) {
      // Block context menu triggered by touch event unless it is right after
      // multi-touch, or we are currently selecting a file.
      if (this.currentList.selectedItem && !this.allowContextMenuByTouch_ &&
          e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents) {
        e.stopPropagation();
      }
    }.bind(this), true);

    // Ensure the list and grid are marked ARIA single select for save as.
    if (type === DialogType.SELECT_SAVEAS_FILE) {
      const list = table.querySelector('#file-list');
      list.setAttribute('aria-multiselectable', 'false');
      grid.setAttribute('aria-multiselectable', 'false');
    }
  }

  /**
   * @return {!FileTable|!FileGrid}
   */
  get currentView() {
    switch (this.currentListType) {
      case ListContainer.ListType.DETAIL:
        return this.table;
      case ListContainer.ListType.THUMBNAIL:
        return this.grid;
    }
    assertNotReached();
  }

  /**
   * @return {!List}
   */
  get currentList() {
    switch (this.currentListType) {
      case ListContainer.ListType.DETAIL:
        return this.table.list;
      case ListContainer.ListType.THUMBNAIL:
        return this.grid;
    }
    assertNotReached();
  }

  /**
   * Notifies beginning of batch update to the UI.
   */
  startBatchUpdates() {
    this.table.startBatchUpdates();
    this.grid.startBatchUpdates();
  }

  /**
   * Notifies end of batch update to the UI.
   */
  endBatchUpdates() {
    this.table.endBatchUpdates();
    this.grid.endBatchUpdates();
  }

  /**
   * Sets the current list type.
   * @param {ListContainer.ListType} listType New list type.
   */
  setCurrentListType(listType) {
    assert(this.dataModel);
    assert(this.selectionModel);

    this.startBatchUpdates();
    this.currentListType = listType;

    this.element.classList.toggle(
        'list-view', listType === ListContainer.ListType.DETAIL);
    this.element.classList.toggle(
        'thumbnail-view', listType === ListContainer.ListType.THUMBNAIL);

    // TODO(dzvorygin): style.display and dataModel setting order shouldn't
    // cause any UI bugs. Currently, the only right way is first to set display
    // style and only then set dataModel.
    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    switch (listType) {
      case ListContainer.ListType.DETAIL:
        this.dataModel.groupByField =
            this.isOnRecent ? GROUP_BY_FIELD_MODIFICATION_TIME : null;
        this.table.dataModel = this.dataModel;
        this.table.setListThumbnailLoader(this.listThumbnailLoader);
        this.table.selectionModel = this.selectionModel;
        this.table.hidden = false;
        this.grid.hidden = true;
        this.grid.selectionModel = this.emptySelectionModel_;
        this.grid.setListThumbnailLoader(null);
        this.grid.dataModel = this.emptyDataModel;
        break;

      case ListContainer.ListType.THUMBNAIL:
        if (this.isOnRecent) {
          this.dataModel.groupByField = GROUP_BY_FIELD_MODIFICATION_TIME;
        } else {
          this.dataModel.groupByField = GROUP_BY_FIELD_DIRECTORY;
        }
        this.grid.dataModel = this.dataModel;
        this.grid.setListThumbnailLoader(this.listThumbnailLoader);
        this.grid.selectionModel = this.selectionModel;
        this.grid.hidden = false;
        this.table.hidden = true;
        this.table.selectionModel = this.emptySelectionModel_;
        this.table.setListThumbnailLoader(null);
        this.table.dataModel = this.emptyDataModel;
        break;

      default:
        assertNotReached();
        break;
    }
    this.endBatchUpdates();
  }

  /**
   * Finds list item element from the ancestor node.
   * @param {!HTMLElement} node
   * @return {ListItem}
   */
  findListItemForNode(node) {
    const item = this.currentList.getListItemAncestor(node);
    // TODO(serya): list should check that.
    return item && this.currentList.isItem(item) ?
        assertInstanceof(item, ListItem) :
        null;
  }

  /**
   * Focuses the active file list in the list container.
   */
  focus() {
    switch (this.currentListType) {
      case ListContainer.ListType.DETAIL:
        this.table.list.focus();
        break;
      case ListContainer.ListType.THUMBNAIL:
        this.grid.focus();
        break;
      default:
        assertNotReached();
        break;
    }
  }

  /**
   * Check if our context menu has any items that can be activated
   * @return {boolean} True if the menu has action item. Otherwise, false.
   * @private
   */
  contextMenuHasActions_() {
    const menu = document.querySelector('#file-context-menu');
    const menuItems = menu.querySelectorAll('cr-menu-item, hr');
    for (const item of menuItems) {
      if (!item.hasAttribute('hidden') && !item.hasAttribute('disabled') &&
          (window.getComputedStyle(item).display != 'none')) {
        return true;
      }
    }
    return false;
  }

  /**
   * Contextmenu event handler to prevent change of focus on long-tapping the
   * header of the file list.
   * @param {!Event} e Menu event.
   * @private
   */
  onContextMenu_(e) {
    if (!this.allowContextMenuByTouch_ && e.sourceCapabilities &&
        e.sourceCapabilities.firesTouchEvents) {
      this.focus();
    }
  }

  /**
   * KeyDown event handler for the div#list-container element.
   * @param {!Event} event Key event.
   * @private
   */
  onKeyDown_(event) {
    // Ignore keydown handler in the rename input box.
    if (event.srcElement.tagName == 'INPUT') {
      event.stopImmediatePropagation();
      return;
    }
  }

  /**
   * KeyPress event handler for the div#list-container element.
   * @param {!Event} event Key event.
   * @private
   */
  onKeyPress_(event) {
    // Ignore keypress handler in the rename input box.
    if (event.srcElement.tagName == 'INPUT' || event.ctrlKey || event.metaKey ||
        event.altKey) {
      event.stopImmediatePropagation();
      return;
    }

    const now = new Date();
    const character = String.fromCharCode(event.charCode).toLowerCase();
    const text =
        now - this.textSearchState.date > 1000 ? '' : this.textSearchState.text;
    this.textSearchState.text = text + character;
    this.textSearchState.date = now;

    if (this.textSearchState.text) {
      dispatchSimpleEvent(this.element, ListContainer.EventType.TEXT_SEARCH);
    }
  }
}

/**
 * @enum {string}
 * @const
 */
ListContainer.EventType = {
  TEXT_SEARCH: 'textsearch',
};

/**
 * @enum {string}
 * @const
 */
ListContainer.ListType = {
  UNINITIALIZED: 'uninitialized',
  DETAIL: 'detail',
  THUMBNAIL: 'thumb',
};

/**
 * Keep the order of this in sync with FileManagerListType in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 *
 * @type {Array<ListContainer.ListType>}
 * @const
 */
ListContainer.ListTypesForUMA = Object.freeze([
  ListContainer.ListType.UNINITIALIZED,
  ListContainer.ListType.DETAIL,
  ListContainer.ListType.THUMBNAIL,
]);
console.assert(
    Object.keys(ListContainer.ListType).length ===
        ListContainer.ListTypesForUMA.length,
    'Members in ListTypesForUMA do not match those in ListType.');

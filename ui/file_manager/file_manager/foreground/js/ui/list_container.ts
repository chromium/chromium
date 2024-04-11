// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert, assertInstanceof, assertNotReached} from 'chrome://resources/js/assert.js';

import {queryRequiredElement} from '../../../common/js/dom_utils.js';
import {DialogType} from '../../../state/state.js';
import type {FileListModel} from '../file_list_model.js';
import {GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME} from '../file_list_model.js';
import type {ListThumbnailLoader} from '../list_thumbnail_loader.js';

import type {FileGrid} from './file_grid.js';
import type {FileListSelectionModel, FileListSingleSelectionModel} from './file_list_selection_model.js';
import type {FileTable} from './file_table.js';
import type {List} from './list.js';
import type {ListItem} from './list_item.js';
import {ListSelectionModel} from './list_selection_model.js';

class TextSearchState {
  text: string = '';
  date: Date = new Date();
}

/**
 * List container for the file table and the grid view.
 */
export class ListContainer {
  currentListType: ListType = ListType.UNINITIALIZED;

  /**
   * The input element to rename entry.
   */
  readonly renameInput: HTMLInputElement;

  /**
   * Spinner on file list which is shown while loading.
   */
  readonly spinner: HTMLElement;

  dataModel: FileListModel|null = null;
  listThumbnailLoader: ListThumbnailLoader|null = null;
  selectionModel: FileListSelectionModel|FileListSingleSelectionModel|null =
      null;

  /**
   * Data model which is used as a placefolder in inactive file list. This is
   * set by FileManager.
   */
  emptyDataModel: FileListModel|null = null;

  /**
   * Selection model which is used as a placefolder in inactive file list.
   */
  private readonly emptySelectionModel_ = new ListSelectionModel();

  readonly textSearchState = new TextSearchState();

  /**
   * Whtehter to allow or cancel a context menu event.
   */
  private allowContextMenuByTouch_ = false;

  /**
   * List container needs to know if the current active directory is Recent
   * or not so it can update groupBy filed accordingly.
   */
  isOnRecent = false;

  /**
   * @param element The container element of the file list.
   * @param table File table.
   * @param grid File grid.
   * @param type The type of the main dialog.
   */
  constructor(
      public readonly element: HTMLElement, public readonly table: FileTable,
      public readonly grid: FileGrid, type: DialogType) {
    this.renameInput = document.createElement('input');
    assertInstanceof(this.renameInput, HTMLInputElement);
    this.renameInput.className = 'rename entry-name';
    this.spinner =
        queryRequiredElement('files-spinner.loading-indicator', element);

    // Overriding the default role 'list' to 'listbox' for better accessibility
    // on ChromeOS.
    this.table.list.setAttribute('role', 'listbox');
    this.table.list.id = 'file-list';
    this.grid.setAttribute('role', 'listbox');
    this.grid.id = 'file-list';

    // Ensure the list and grid are marked ARIA single select for save as.
    if (type === DialogType.SELECT_SAVEAS_FILE) {
      const list = table.querySelector('#file-list');
      list?.setAttribute('aria-multiselectable', 'false');
      grid.setAttribute('aria-multiselectable', 'false');
    }
  }

  /**
   * Avoid adding event listeners in the constructor because the ListContainer
   * isn't fully usable until setCurrentListType() is called.
   */
  private addEventListeners_() {
    this.element.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.element.addEventListener('keypress', this.onKeyPress_.bind(this));
    this.element.addEventListener(
        'contextmenu', this.onContextMenu_.bind(this), /* useCapture */ true);

    // Disables context menu by long-tap when long-tap would transition to
    // multi-select mode, but keep it enabled for two-finger tap.
    this.element.addEventListener('touchstart', (e: TouchEvent) => {
      if (e.touches.length > 1 || this.currentList.selectedItem) {
        this.allowContextMenuByTouch_ = true;
      }
    }, {passive: true});
    this.element.addEventListener('touchend', (e: TouchEvent) => {
      if (e.touches.length === 0) {
        // contextmenu event will be sent right after touchend.
        setTimeout(() => this.allowContextMenuByTouch_ = false);
      }
    });
  }

  get currentView(): FileTable|FileGrid {
    switch (this.currentListType) {
      case ListType.DETAIL:
        return this.table;
      case ListType.THUMBNAIL:
        return this.grid;
    }
    assertNotReached();
  }

  get currentList(): List {
    switch (this.currentListType) {
      case ListType.DETAIL:
        return this.table.list;
      case ListType.THUMBNAIL:
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
   * @param listType New list type.
   */
  setCurrentListType(listType: ListType) {
    assert(this.dataModel);
    assert(this.selectionModel);

    this.addEventListeners_();

    this.startBatchUpdates();
    this.currentListType = listType;

    this.element.classList.toggle('list-view', listType === ListType.DETAIL);
    this.element.classList.toggle(
        'thumbnail-view', listType === ListType.THUMBNAIL);

    // TODO(dzvorygin): style.display and dataModel setting order shouldn't
    // cause any UI bugs. Currently, the only right way is first to set display
    // style and only then set dataModel.
    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    switch (listType) {
      case ListType.DETAIL:
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

      case ListType.THUMBNAIL:
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
    }
    this.endBatchUpdates();
  }

  /**
   * Finds list item element from the ancestor node.
   */
  findListItemForNode(node: HTMLElement): ListItem|null {
    const item = this.currentList.getListItemAncestor(node);
    return item && this.currentList.isItem(item) ? item : null;
  }

  /**
   * Focuses the active file list in the list container.
   */
  focus() {
    switch (this.currentListType) {
      case ListType.DETAIL:
        this.table.list.focus();
        break;
      case ListType.THUMBNAIL:
        this.grid.focus();
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Contextmenu event handler to prevent change of focus on long-tapping the
   * header of the file list.
   * @param e Menu event.
   */
  private onContextMenu_(e: Event) {
    // sourceCapabilities isn't defined in TS, because it's experimental.
    const sourceCapabilities = (e as any).sourceCapabilities;

    // Block context menu triggered by touch event unless either:
    // - It is right after a multi-touch, or
    // - We were already in multi-select mode, or
    // - No items are selected (i.e. long-tap on empty area in the current
    // folder).
    if (this.currentList.selectedItem && !this.allowContextMenuByTouch_ &&
        sourceCapabilities && sourceCapabilities.firesTouchEvents) {
      e.stopPropagation();
      e.preventDefault();
    }
    if (!this.allowContextMenuByTouch_ && sourceCapabilities &&
        sourceCapabilities.firesTouchEvents) {
      this.focus();
    }
  }

  /**
   * KeyDown event handler for the div#list-container element.
   * @param event Key event.
   */
  private onKeyDown_(event: Event) {
    // Ignore keydown handler in the rename input box.
    const srcElement = event.srcElement as HTMLElement | null;
    if (srcElement?.tagName === 'INPUT') {
      event.stopImmediatePropagation();
      return;
    }
  }

  /**
   * KeyPress event handler for the div#list-container element.
   * @param event Key event.
   */
  private onKeyPress_(event: KeyboardEvent) {
    const srcElement = event.srcElement as HTMLElement | null;

    // Ignore keypress handler in the rename input box.
    if (srcElement?.tagName === 'INPUT' || event.ctrlKey || event.metaKey ||
        event.altKey) {
      event.stopImmediatePropagation();
      return;
    }

    const now = new Date();
    const character = String.fromCharCode(event.charCode).toLowerCase();
    const text = Number(now) - Number(this.textSearchState.date) > 1000 ?
        '' :
        this.textSearchState.text;
    this.textSearchState.text = text + character;
    this.textSearchState.date = now;

    if (this.textSearchState.text) {
      dispatchSimpleEvent(this.element, EventType.TEXT_SEARCH);
    }
  }
}

export enum EventType {
  TEXT_SEARCH = 'textsearch',
}

export enum ListType {
  UNINITIALIZED = 'uninitialized',
  DETAIL = 'detail',
  THUMBNAIL = 'thumb',
}

/**
 * Keep the order of this in sync with FileManagerListType in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 */
export const ListTypesForUMA = Object.freeze([
  ListType.UNINITIALIZED,
  ListType.DETAIL,
  ListType.THUMBNAIL,
]);
console.assert(
    Object.keys(ListType).length === ListTypesForUMA.length,
    'Members in ListTypesForUMA do not match those in ListType.');

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table control.
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import {type ArrayDataModel} from '../../../../common/js/array_data_model.js';
import {boolAttrSetter, convertToKebabCase, crInjectTypeAndInit} from '../../../../common/js/cr_ui.js';
import {List} from '../list.js';
import {type ListItem} from '../list_item.js';
import {ListSelectionModel} from '../list_selection_model.js';
import {type ListSingleSelectionModel} from '../list_single_selection_model.js';

import {TableColumnModel} from './table_column_model.js';
import {TableHeader} from './table_header.js';
import {TableList} from './table_list.js';


type RenderFunction = (item: any, table: Table) => ListItem;
/**
 * Creates a new table element.
 */
export class Table extends HTMLDivElement {
  private columnModel_: TableColumnModel|null = null;
  protected list_: TableList|null = null;
  private header_: TableHeader|null = null;
  private boundHandleChangeList_: null|((e: Event|null) => void) = null;
  private boundHandleSorted_: null|((e: Event|null) => void) = null;
  private boundResize_: null|((e: Event|null) => void) = null;

  /**
   * The table data model.
   *
   */
  get dataModel(): ArrayDataModel|null {
    return this.list.dataModel;
  }

  set dataModel(dataModel: ArrayDataModel|null) {
    assert(this.list_);
    if (this.list_.dataModel !== dataModel) {
      if (this.list_.dataModel) {
        this.list_.dataModel.removeEventListener(
            'sorted', this.boundHandleSorted_);
        this.list_.dataModel.removeEventListener(
            'change', this.boundHandleChangeList_);
        this.list_.dataModel.removeEventListener(
            'splice', this.boundHandleChangeList_);
      }
      this.list_.dataModel = dataModel;
      if (this.list_.dataModel) {
        this.list_.dataModel.addEventListener(
            'sorted', this.boundHandleSorted_);
        this.list_.dataModel.addEventListener(
            'change', this.boundHandleChangeList_);
        this.list_.dataModel.addEventListener(
            'splice', this.boundHandleChangeList_);
      }
      this.header.redraw();
    }
  }

  /**
   * The list of table.
   *
   */
  get list() {
    return this.list_!;
  }

  /**
   * The table column model.
   *
   */
  get columnModel() {
    return this.columnModel_!;
  }

  set columnModel(columnModel) {
    if (this.columnModel_ !== columnModel) {
      if (this.columnModel_) {
        this.columnModel_.removeEventListener('resize', this.boundResize_);
      }
      this.columnModel_ = columnModel;

      if (this.columnModel_) {
        this.columnModel_.addEventListener('resize', this.boundResize_);
      }
      this.list.invalidate();
      this.redraw();
    }
  }

  /**
   * The table selection model.
   */
  get selectionModel(): ListSelectionModel|ListSingleSelectionModel {
    return this.list.selectionModel!;
  }

  set selectionModel(selectionModel: ListSelectionModel|
                     ListSingleSelectionModel) {
    if (this.list.selectionModel !== selectionModel) {
      if (this.dataModel) {
        selectionModel.adjustLength(this.dataModel.length);
      }
      // TODO: Remove the type cast when ListSingleSelectionModel is converted
      // to TS.
      this.list_!.selectionModel = selectionModel as ListSelectionModel;
    }
  }

  /**
   * The accessor to "autoExpands" property of the list.
   *
   */
  get autoExpands() {
    return this.list.autoExpands;
  }

  set autoExpands(autoExpands) {
    this.list.autoExpands = autoExpands;
  }

  get fixedHeight(): boolean {
    return this.list.fixedHeight;
  }

  set fixedHeight(fixedHeight: boolean) {
    this.list.fixedHeight = fixedHeight;
  }

  /**
   * Returns render function for row.
   * @return Render function.
   */
  getRenderFunction(): RenderFunction {
    return this.renderFunction_;
  }

  private renderFunction_(dataItem: unknown, table: Table) {
    // `this` must not be accessed here, since it may be anything, especially
    // not a pointer to this object.

    const cm = table.columnModel;
    const listItem = List.prototype.createItem.call(table.list, '');
    listItem.className = 'table-row';

    for (let i = 0; i < cm.size; i++) {
      const cell = table.ownerDocument.createElement('div');
      cell.style.width = cm.getWidth(i) + 'px';
      cell.className = 'table-row-cell';
      if (cm.isEndAlign(i)) {
        cell.style.textAlign = 'end';
      }
      cell.hidden = !cm.isVisible(i);
      cell.appendChild(
          cm.getRenderFunction(i).call(null, dataItem, cm.getId(i)!, table));

      listItem.appendChild(cell);
    }
    listItem.style.width = cm.totalWidth + 'px';

    return listItem;
  }

  /**
   * Sets render function for row.
   * @param renderFunction Render function.
   */
  setRenderFunction(renderFunction: RenderFunction) {
    if (renderFunction === this.renderFunction_) {
      return;
    }

    this.renderFunction_ = renderFunction;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * The header of the table.
   *
   */
  get header() {
    return this.header_!;
  }

  /**
   * Initializes the element.
   */
  initialize() {
    this.columnModel_ = new TableColumnModel([]);
    this.header_ = this.ownerDocument.createElement('div') as TableHeader;
    this.list_ = this.ownerDocument.createElement('list') as TableList;

    this.appendChild(this.header_);
    this.appendChild(this.list_);

    crInjectTypeAndInit(this.list_, TableList);
    this.list_.selectionModel = new ListSelectionModel();
    this.list_.table = this;
    this.list_.addEventListener('scroll', this.handleScroll_.bind(this));

    crInjectTypeAndInit(this.header_, TableHeader);
    this.header_.table = this;

    this.classList.add('table');

    this.boundResize_ = this.resize.bind(this);
    this.boundHandleSorted_ = this.handleSorted_.bind(this);
    this.boundHandleChangeList_ = this.handleChangeList_.bind(this);

    // The contained list should be focusable, not the table itself.
    if (this.hasAttribute('tabindex')) {
      this.list_.setAttribute('tabindex', this.getAttribute('tabindex')!);
      this.removeAttribute('tabindex');
    }

    this.addEventListener('focus', this.handleElementFocus_, true);
    this.addEventListener('blur', this.handleElementBlur_, true);
  }

  /**
   * Redraws the table.
   */
  redraw() {
    this.list.redraw();
    this.header.redraw();
  }

  startBatchUpdates() {
    this.list.startBatchUpdates();
    this.header.startBatchUpdates();
  }

  endBatchUpdates() {
    this.list.endBatchUpdates();
    this.header.endBatchUpdates();
  }

  /**
   * Resize the table columns.
   */
  resize() {
    // We resize columns only instead of full redraw.
    this.list.resize();
    this.header.resize();
  }

  /**
   * Ensures that a given index is inside the viewport.
   * @param i The index of the item to scroll into view.
   */
  scrollIndexIntoView(i: number) {
    this.list.scrollIndexIntoView(i);
  }

  /**
   * Find the list item element at the given index.
   * @param index The index of the list item to get.
   * @return The found list item or null if not found.
   */
  getListItemByIndex(index: number): ListItem|null {
    return this.list.getListItemByIndex(index);
  }

  /**
   * This handles data model 'sorted' event.
   * After sorting we need to redraw header
   * @param e The 'sorted' event.
   */
  private handleSorted_(_e: Event|null) {
    this.header.redraw();
    // If we have 'focus-outline-visible' on the root HTML element and focus
    // has reverted to the body element it means this sort header creation
    // was the result of a keyboard action so set focus to the (newly
    // recreated) sort button in that case.
    if (document.querySelector('html.focus-outline-visible') &&
        (document.activeElement instanceof HTMLBodyElement)) {
      const sortButton = this.header.querySelector<HTMLElement>(
          'cr-icon-button[tabindex="0"]');
      if (sortButton) {
        sortButton.focus();
      }
    }
    this.onDataModelSorted();
  }

  /**
   * Override to inject custom logic after data model sorting is done.
   */
  protected onDataModelSorted() {}

  /**
   * This handles data model 'change' and 'splice' events.
   * Since they may change the visibility of scrollbar, table may need to
   * re-calculation the width of column headers.
   * @param e The 'change' or 'splice' event.
   */
  private handleChangeList_(_e: Event|null) {
    requestAnimationFrame(this.header.updateWidth.bind(this.header_));
  }

  /**
   * This handles list 'scroll' events. Scrolls the header accordingly.
   * @param _e Scroll event.
   */
  private handleScroll_(_e: Event) {
    this.header.style.marginLeft = -this.list_!.scrollLeft + 'px';
  }

  /**
   * Sort data by the given column.
   * @param i The index of the column to sort by.
   */
  sort(i: number) {
    const cm = this.columnModel_!;
    const sortStatus = this.list.dataModel!.sortStatus;
    if (sortStatus.field === cm.getId(i)) {
      const sortDirection = sortStatus.direction === 'desc' ? 'asc' : 'desc';
      this.list.dataModel!.sort(sortStatus.field, sortDirection);
    } else {
      this.list.dataModel!.sort(cm.getId(i)!, cm.getDefaultOrder(i));
    }
    if (this.selectionModel.selectedIndex === -1) {
      this.list.scrollTop = 0;
    }
  }

  /**
   * Called when an element in the table is focused. Marks the table as having
   * a focused element, and dispatches an event if it didn't have focus.
   * @param e The focus event.
   */
  private handleElementFocus_(_e: Event) {
    if (!this.hasElementFocus) {
      this.hasElementFocus = true;
      // Force styles based on hasElementFocus to take effect.
      this.list.redraw();
    }
  }

  /**
   * Called when an element in the table is blurred. If focus moves outside
   * the table, marks the table as no longer having focus and dispatches an
   * event.
   * @param e The blur event.
   */
  private handleElementBlur_(e: Event) {
    // When the blur event happens we do not know who is getting focus so we
    // delay this a bit until we know if the new focus node is outside the
    // table.
    const doc = (e.target as HTMLElement).ownerDocument;
    window.setTimeout(() => {
      const activeElement = doc.activeElement;
      if (!this.contains(activeElement)) {
        this.hasElementFocus = false;
        // Force styles based on hasElementFocus to take effect.
        this.list.redraw();
      }
    });
  }

  /**
   * Adjust column width to fit its content.
   * @param index Index of the column to adjust width.
   */
  fitColumn(index: number) {
    const list = this.list;
    const listHeight = list.clientHeight;

    assert(this.dataModel);
    assert(this.columnModel_);
    const cm = this.columnModel_;
    const dm = this.dataModel;
    const columnId = cm.getId(index)!;
    const doc = this.ownerDocument;
    const render = cm.getRenderFunction(index);
    const table = this;
    const MAXIMUM_ROWS_TO_MEASURE = 1000;

    // Create a temporaty list item, put all cells into it and measure its
    // width. Then remove the item. It fits "list > *" CSS rules.
    const container = doc.createElement('li');
    container.style.display = 'inline-block';
    container.style.textAlign = 'start';
    // The container will have width of the longest cell.
    container.style.webkitBoxOrient = 'vertical';

    // Ensure all needed data available.
    // Select at most MAXIMUM_ROWS_TO_MEASURE items around visible area.
    const items = list.getItemsInViewPort(list.scrollTop, listHeight);
    const firstIndex = Math.floor(
        Math.max(0, (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
    const lastIndex = Math.min(dm.length, firstIndex + MAXIMUM_ROWS_TO_MEASURE);
    for (let i = firstIndex; i < lastIndex; i++) {
      const item = dm.item(i);
      const div = doc.createElement('div');
      div.className = 'table-row-cell';
      div.appendChild(render(item, columnId, table));
      container.appendChild(div);
    }
    list.appendChild(container);
    const width = parseFloat(window.getComputedStyle(container).width);
    list.removeChild(container);
    cm.setWidth(index, width);
  }

  normalizeColumns() {
    this.columnModel.normalizeWidths(this.clientWidth);
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
  }

  /**
   * Whether the table or one of its descendants has focus. This is necessary
   * because table contents can contain controls that can be focused, and for
   * some purposes (e.g., styling), the table can still be conceptually focused
   * at that point even though it doesn't actually have the page focus.
   */
  get hasElementFocus(): boolean {
    return this.hasAttribute(convertToKebabCase('hasElementFocus'));
  }

  set hasElementFocus(value: boolean) {
    boolAttrSetter(this, 'hasElementFocus', value);
  }
}

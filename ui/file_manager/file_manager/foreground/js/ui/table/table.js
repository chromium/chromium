// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table control.
 */

import {dispatchSimpleEvent, getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';

import {ArrayDataModel} from '../../../../common/js/array_data_model.js';
import {List} from '../list.js';
import {ListItem} from '../list_item.js';
import {ListSelectionModel} from '../list_selection_model.js';
import {ListSingleSelectionModel} from '../list_single_selection_model.js';

import {TableColumnModel} from './table_column_model.js';
import {TableHeader} from './table_header.js';
import {TableList} from './table_list.js';

/**
 * Creates a new table element.
 * @extends {HTMLDivElement}
 */
export class Table {
  constructor() {
    /** @private {TableColumnModel} */
    this.columnModel_;

    /** @protected {?TableList} */
    this.list_;

    /** @private {TableHeader} */
    this.header_;

    /** @private {function((Event|null))} */
    this.boundHandleChangeList_;

    /** @private {function((Event|null))} */
    this.boundHandleSorted_;

    /** @private {function((Event|null))} */
    this.boundResize_;
  }

  /**
   * The table data model.
   *
   * @type {ArrayDataModel}
   */
  get dataModel() {
    return this.list_.dataModel;
  }
  set dataModel(dataModel) {
    if (this.list_.dataModel != dataModel) {
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
      this.header_.redraw();
    }
  }

  /**
   * The list of table.
   *
   * @type {List}
   */
  get list() {
    return this.list_;
  }

  /**
   * The table column model.
   *
   * @type {TableColumnModel}
   */
  get columnModel() {
    return this.columnModel_;
  }
  set columnModel(columnModel) {
    if (this.columnModel_ != columnModel) {
      if (this.columnModel_) {
        this.columnModel_.removeEventListener('resize', this.boundResize_);
      }
      this.columnModel_ = columnModel;

      if (this.columnModel_) {
        this.columnModel_.addEventListener('resize', this.boundResize_);
      }
      this.list_.invalidate();
      this.redraw();
    }
  }

  /**
   * The table selection model.
   *
   * @type
   * {ListSelectionModel|ListSingleSelectionModel}
   */
  get selectionModel() {
    return this.list_.selectionModel;
  }
  set selectionModel(selectionModel) {
    if (this.list_.selectionModel != selectionModel) {
      if (this.dataModel) {
        selectionModel.adjustLength(this.dataModel.length);
      }
      this.list_.selectionModel = selectionModel;
    }
  }

  /**
   * The accessor to "autoExpands" property of the list.
   *
   * @type {boolean}
   */
  get autoExpands() {
    return this.list_.autoExpands;
  }
  set autoExpands(autoExpands) {
    this.list_.autoExpands = autoExpands;
  }

  get fixedHeight() {
    return this.list_.fixedHeight;
  }
  set fixedHeight(fixedHeight) {
    this.list_.fixedHeight = fixedHeight;
  }

  /**
   * Returns render function for row.
   * @return {function(*, Table): HTMLElement} Render function.
   */
  getRenderFunction() {
    return this.renderFunction_;
  }

  /**
   * @private
   */
  renderFunction_(dataItem, table) {
    // `This` must not be accessed here, since it may be anything, especially
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
          cm.getRenderFunction(i).call(null, dataItem, cm.getId(i), table));

      listItem.appendChild(cell);
    }
    listItem.style.width = cm.totalWidth + 'px';

    return listItem;
  }

  /**
   * Sets render function for row.
   * @param {function(*, Table): HTMLElement} renderFunction Render
   *     function.
   */
  setRenderFunction(renderFunction) {
    if (renderFunction === this.renderFunction_) {
      return;
    }

    this.renderFunction_ = renderFunction;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * The header of the table.
   *
   * @type {TableColumnModel}
   */
  get header() {
    return this.header_;
  }

  /**
   * Initializes the element.
   * @param {Element} element
   */
  static decorate(element) {
    element.__proto__ = Table.prototype;
    element = /** @type {Table} */ (element);

    element.columnModel_ = new TableColumnModel([]);
    element.header_ =
        /** @type {TableHeader} */ (element.ownerDocument.createElement('div'));
    element.list_ =
        /** @type {TableList} */ (element.ownerDocument.createElement('list'));

    element.appendChild(element.header_);
    element.appendChild(element.list_);

    TableList.decorate(element.list_);
    element.list_.selectionModel = new ListSelectionModel();
    element.list_.table = element;
    element.list_.addEventListener(
        'scroll', element.handleScroll_.bind(element));

    TableHeader.decorate(element.header_);
    element.header_.table = element;

    element.classList.add('table');

    element.boundResize_ = element.resize.bind(element);
    element.boundHandleSorted_ = element.handleSorted_.bind(element);
    element.boundHandleChangeList_ = element.handleChangeList_.bind(element);

    // The contained list should be focusable, not the table itself.
    if (element.hasAttribute('tabindex')) {
      element.list_.setAttribute('tabindex', element.getAttribute('tabindex'));
      element.removeAttribute('tabindex');
    }

    element.addEventListener('focus', element.handleElementFocus_, true);
    element.addEventListener('blur', element.handleElementBlur_, true);
  }

  /**
   * Redraws the table.
   */
  redraw() {
    this.list_.redraw();
    this.header_.redraw();
  }

  startBatchUpdates() {
    this.list_.startBatchUpdates();
    this.header_.startBatchUpdates();
  }

  endBatchUpdates() {
    this.list_.endBatchUpdates();
    this.header_.endBatchUpdates();
  }

  /**
   * Resize the table columns.
   */
  resize() {
    // We resize columns only instead of full redraw.
    this.list_.resize();
    this.header_.resize();
  }

  /**
   * Ensures that a given index is inside the viewport.
   * @param {number} i The index of the item to scroll into view.
   */
  scrollIndexIntoView(i) {
    this.list_.scrollIndexIntoView(i);
  }

  /**
   * Find the list item element at the given index.
   * @param {number} index The index of the list item to get.
   * @return {ListItem} The found list item or null if not found.
   */
  getListItemByIndex(index) {
    return this.list_.getListItemByIndex(index);
  }

  /**
   * This handles data model 'sorted' event.
   * After sorting we need to redraw header
   * @param {Event} e The 'sorted' event.
   */
  handleSorted_(e) {
    this.header_.redraw();
    // If we have 'focus-outline-visible' on the root HTML element and focus
    // has reverted to the body element it means this sort header creation
    // was the result of a keyboard action so set focus to the (newly
    // recreated) sort button in that case.
    if (document.querySelector('html.focus-outline-visible') &&
        (document.activeElement instanceof HTMLBodyElement)) {
      const sortButton =
          this.header_.querySelector('cr-icon-button[tabindex="0"]');
      if (sortButton) {
        sortButton.focus();
      }
    }
    this.onDataModelSorted();
  }

  /**
   * Override to inject custom logic after data model sorting is done.
   * @protected
   */
  onDataModelSorted() {}

  /**
   * This handles data model 'change' and 'splice' events.
   * Since they may change the visibility of scrollbar, table may need to
   * re-calculation the width of column headers.
   * @param {Event} e The 'change' or 'splice' event.
   */
  handleChangeList_(e) {
    requestAnimationFrame(this.header_.updateWidth.bind(this.header_));
  }

  /**
   * This handles list 'scroll' events. Scrolls the header accordingly.
   * @param {Event} e Scroll event.
   */
  handleScroll_(e) {
    this.header_.style.marginLeft = -this.list_.scrollLeft + 'px';
  }

  /**
   * Sort data by the given column.
   * @param {number} i The index of the column to sort by.
   */
  sort(i) {
    const cm = this.columnModel_;
    const sortStatus = this.list_.dataModel.sortStatus;
    if (sortStatus.field == cm.getId(i)) {
      const sortDirection = sortStatus.direction == 'desc' ? 'asc' : 'desc';
      this.list_.dataModel.sort(sortStatus.field, sortDirection);
    } else {
      this.list_.dataModel.sort(cm.getId(i), cm.getDefaultOrder(i));
    }
    if (this.selectionModel.selectedIndex == -1) {
      this.list_.scrollTop = 0;
    }
  }

  /**
   * Called when an element in the table is focused. Marks the table as having
   * a focused element, and dispatches an event if it didn't have focus.
   * @param {Event} e The focus event.
   * @private
   */
  handleElementFocus_(e) {
    if (!this.hasElementFocus) {
      this.hasElementFocus = true;
      // Force styles based on hasElementFocus to take effect.
      this.list_.redraw();
    }
  }

  /**
   * Called when an element in the table is blurred. If focus moves outside
   * the table, marks the table as no longer having focus and dispatches an
   * event.
   * @param {Event} e The blur event.
   * @private
   */
  handleElementBlur_(e) {
    // When the blur event happens we do not know who is getting focus so we
    // delay this a bit until we know if the new focus node is outside the
    // table.
    const table = this;
    const list = this.list_;
    const doc = e.target.ownerDocument;
    window.setTimeout(function() {
      const activeElement = doc.activeElement;
      if (!table.contains(activeElement)) {
        table.hasElementFocus = false;
        // Force styles based on hasElementFocus to take effect.
        list.redraw();
      }
    }, 0);
  }

  /**
   * Adjust column width to fit its content.
   * @param {number} index Index of the column to adjust width.
   */
  fitColumn(index) {
    const list = this.list_;
    const listHeight = list.clientHeight;

    const cm = this.columnModel_;
    const dm = this.dataModel;
    const columnId = cm.getId(index);
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
    dm.prepareSort(columnId, function() {
      // Select at most MAXIMUM_ROWS_TO_MEASURE items around visible area.
      const items = list.getItemsInViewPort(list.scrollTop, listHeight);
      const firstIndex = Math.floor(Math.max(
          0, (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
      const lastIndex =
          Math.min(dm.length, firstIndex + MAXIMUM_ROWS_TO_MEASURE);
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
    });
  }

  normalizeColumns() {
    this.columnModel.normalizeWidths(this.clientWidth);
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
  }
}

Table.prototype.__proto__ = HTMLDivElement.prototype;


/**
 * Whether the table or one of its descendants has focus. This is necessary
 * because table contents can contain controls that can be focused, and for
 * some purposes (e.g., styling), the table can still be conceptually focused
 * at that point even though it doesn't actually have the page focus.
 * @type {boolean}
 */
Table.prototype.hasElementFocus;
Object.defineProperty(
    Table.prototype, 'hasElementFocus',
    getPropertyDescriptor('hasElementFocus', PropertyKind.BOOL_ATTR));

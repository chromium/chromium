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
    /** @private @type {TableColumnModel} */
    this.columnModel_;

    /** @protected @type {?TableList} */
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_;

    /** @private @type {TableHeader} */
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {function((Event|null))} */
    // @ts-ignore: error TS2551: Property 'boundHandleChangeList_' does not
    // exist on type 'Table'. Did you mean 'handleChangeList_'?
    this.boundHandleChangeList_;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {function((Event|null))} */
    // @ts-ignore: error TS2551: Property 'boundHandleSorted_' does not exist on
    // type 'Table'. Did you mean 'handleSorted_'?
    this.boundHandleSorted_;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {function((Event|null))} */
    // @ts-ignore: error TS2339: Property 'boundResize_' does not exist on type
    // 'Table'.
    this.boundResize_;
  }

  /**
   * The table data model.
   *
   * @type {ArrayDataModel}
   */
  get dataModel() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    return this.list_.dataModel;
  }
  set dataModel(dataModel) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    if (this.list_.dataModel != dataModel) {
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      if (this.list_.dataModel) {
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.removeEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleSorted_' does not
            // exist on type 'Table'. Did you mean 'handleSorted_'?
            'sorted', this.boundHandleSorted_);
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.removeEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleChangeList_' does
            // not exist on type 'Table'. Did you mean 'handleChangeList_'?
            'change', this.boundHandleChangeList_);
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.removeEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleChangeList_' does
            // not exist on type 'Table'. Did you mean 'handleChangeList_'?
            'splice', this.boundHandleChangeList_);
      }
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      this.list_.dataModel = dataModel;
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      if (this.list_.dataModel) {
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.addEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleSorted_' does not
            // exist on type 'Table'. Did you mean 'handleSorted_'?
            'sorted', this.boundHandleSorted_);
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.addEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleChangeList_' does
            // not exist on type 'Table'. Did you mean 'handleChangeList_'?
            'change', this.boundHandleChangeList_);
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'Table'. Did you mean 'list'?
        this.list_.dataModel.addEventListener(
            // @ts-ignore: error TS2551: Property 'boundHandleChangeList_' does
            // not exist on type 'Table'. Did you mean 'handleChangeList_'?
            'splice', this.boundHandleChangeList_);
      }
      // @ts-ignore: error TS2551: Property 'header_' does not exist on type
      // 'Table'. Did you mean 'header'?
      this.header_.redraw();
    }
  }

  /**
   * The list of table.
   *
   * @type {List}
   */
  get list() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
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
        // @ts-ignore: error TS2339: Property 'boundResize_' does not exist on
        // type 'Table'.
        this.columnModel_.removeEventListener('resize', this.boundResize_);
      }
      // @ts-ignore: error TS7022: 'columnModel_' implicitly has type 'any'
      // because it does not have a type annotation and is referenced directly
      // or indirectly in its own initializer.
      this.columnModel_ = columnModel;

      if (this.columnModel_) {
        // @ts-ignore: error TS2339: Property 'boundResize_' does not exist on
        // type 'Table'.
        this.columnModel_.addEventListener('resize', this.boundResize_);
      }
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
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
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    return this.list_.selectionModel;
  }
  set selectionModel(selectionModel) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    if (this.list_.selectionModel != selectionModel) {
      if (this.dataModel) {
        selectionModel.adjustLength(this.dataModel.length);
      }
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      this.list_.selectionModel = selectionModel;
    }
  }

  /**
   * The accessor to "autoExpands" property of the list.
   *
   * @type {boolean}
   */
  get autoExpands() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    return this.list_.autoExpands;
  }
  set autoExpands(autoExpands) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.autoExpands = autoExpands;
  }

  get fixedHeight() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    return this.list_.fixedHeight;
  }
  set fixedHeight(fixedHeight) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
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
  // @ts-ignore: error TS7006: Parameter 'table' implicitly has an 'any' type.
  renderFunction_(dataItem, table) {
    // `This` must not be accessed here, since it may be anything, especially
    // not a pointer to this object.

    const cm = table.columnModel;
    // @ts-ignore: error TS2339: Property 'createItem' does not exist on type
    // 'List'.
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
   * @param {function(*, Table): ListItem} renderFunction Render
   *     function.
   */
  setRenderFunction(renderFunction) {
    if (renderFunction === this.renderFunction_) {
      return;
    }

    this.renderFunction_ = renderFunction;
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * The header of the table.
   *
   * @type {TableColumnModel}
   */
  get header() {
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    return this.header_;
  }

  /**
   * Initializes the element.
   * @param {Element} element
   */
  static decorate(element) {
    // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
    // 'Element'.
    element.__proto__ = Table.prototype;
    // @ts-ignore: error TS2352: Conversion of type 'Element' to type 'Table'
    // may be a mistake because neither type sufficiently overlaps with the
    // other. If this was intentional, convert the expression to 'unknown'
    // first.
    element = /** @type {Table} */ (element);

    // @ts-ignore: error TS2339: Property 'columnModel_' does not exist on type
    // 'Element'.
    element.columnModel_ = new TableColumnModel([]);
    // @ts-ignore: error TS2339: Property 'header_' does not exist on type
    // 'Element'.
    element.header_ =
        // @ts-ignore: error TS2352: Conversion of type 'HTMLDivElement' to type
        // 'TableHeader' may be a mistake because neither type sufficiently
        // overlaps with the other. If this was intentional, convert the
        // expression to 'unknown' first.
        /** @type {TableHeader} */ (element.ownerDocument.createElement('div'));
    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    element.list_ =
        /** @type {TableList} */ (element.ownerDocument.createElement('list'));

    // @ts-ignore: error TS2339: Property 'header_' does not exist on type
    // 'Element'.
    element.appendChild(element.header_);
    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    element.appendChild(element.list_);

    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    TableList.decorate(element.list_);
    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    element.list_.selectionModel = new ListSelectionModel();
    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    element.list_.table = element;
    // @ts-ignore: error TS2339: Property 'list_' does not exist on type
    // 'Element'.
    element.list_.addEventListener(
        // @ts-ignore: error TS2339: Property 'handleScroll_' does not exist on
        // type 'Element'.
        'scroll', element.handleScroll_.bind(element));

    // @ts-ignore: error TS2339: Property 'header_' does not exist on type
    // 'Element'.
    TableHeader.decorate(element.header_);
    // @ts-ignore: error TS2339: Property 'header_' does not exist on type
    // 'Element'.
    element.header_.table = element;

    element.classList.add('table');

    // @ts-ignore: error TS2339: Property 'resize' does not exist on type
    // 'Element'.
    element.boundResize_ = element.resize.bind(element);
    // @ts-ignore: error TS2339: Property 'handleSorted_' does not exist on type
    // 'Element'.
    element.boundHandleSorted_ = element.handleSorted_.bind(element);
    // @ts-ignore: error TS2339: Property 'handleChangeList_' does not exist on
    // type 'Element'.
    element.boundHandleChangeList_ = element.handleChangeList_.bind(element);

    // The contained list should be focusable, not the table itself.
    if (element.hasAttribute('tabindex')) {
      // @ts-ignore: error TS2339: Property 'list_' does not exist on type
      // 'Element'.
      element.list_.setAttribute('tabindex', element.getAttribute('tabindex'));
      element.removeAttribute('tabindex');
    }

    // @ts-ignore: error TS2339: Property 'handleElementFocus_' does not exist
    // on type 'Element'.
    element.addEventListener('focus', element.handleElementFocus_, true);
    // @ts-ignore: error TS2339: Property 'handleElementBlur_' does not exist on
    // type 'Element'.
    element.addEventListener('blur', element.handleElementBlur_, true);
  }

  /**
   * Redraws the table.
   */
  redraw() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.redraw();
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_.redraw();
  }

  startBatchUpdates() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.startBatchUpdates();
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_.startBatchUpdates();
  }

  endBatchUpdates() {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.endBatchUpdates();
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_.endBatchUpdates();
  }

  /**
   * Resize the table columns.
   */
  resize() {
    // We resize columns only instead of full redraw.
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.resize();
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_.resize();
  }

  /**
   * Ensures that a given index is inside the viewport.
   * @param {number} i The index of the item to scroll into view.
   */
  scrollIndexIntoView(i) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.list_.scrollIndexIntoView(i);
  }

  /**
   * Find the list item element at the given index.
   * @param {number} index The index of the list item to get.
   * @return {ListItem} The found list item or null if not found.
   */
  getListItemByIndex(index) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    return this.list_.getListItemByIndex(index);
  }

  /**
   * This handles data model 'sorted' event.
   * After sorting we need to redraw header
   * @param {Event} e The 'sorted' event.
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleSorted_(e) {
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    this.header_.redraw();
    // If we have 'focus-outline-visible' on the root HTML element and focus
    // has reverted to the body element it means this sort header creation
    // was the result of a keyboard action so set focus to the (newly
    // recreated) sort button in that case.
    if (document.querySelector('html.focus-outline-visible') &&
        (document.activeElement instanceof HTMLBodyElement)) {
      const sortButton =
          // @ts-ignore: error TS2551: Property 'header_' does not exist on type
          // 'Table'. Did you mean 'header'?
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
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleChangeList_(e) {
    // @ts-ignore: error TS2551: Property 'header_' does not exist on type
    // 'Table'. Did you mean 'header'?
    requestAnimationFrame(this.header_.updateWidth.bind(this.header_));
  }

  /**
   * This handles list 'scroll' events. Scrolls the header accordingly.
   * @param {Event} e Scroll event.
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleScroll_(e) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    this.header_.style.marginLeft = -this.list_.scrollLeft + 'px';
  }

  /**
   * Sort data by the given column.
   * @param {number} i The index of the column to sort by.
   */
  sort(i) {
    const cm = this.columnModel_;
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    const sortStatus = this.list_.dataModel.sortStatus;
    // @ts-ignore: error TS2339: Property 'field' does not exist on type
    // 'Object'.
    if (sortStatus.field == cm.getId(i)) {
      // @ts-ignore: error TS2339: Property 'direction' does not exist on type
      // 'Object'.
      const sortDirection = sortStatus.direction == 'desc' ? 'asc' : 'desc';
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      this.list_.dataModel.sort(sortStatus.field, sortDirection);
    } else {
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      this.list_.dataModel.sort(cm.getId(i), cm.getDefaultOrder(i));
    }
    if (this.selectionModel.selectedIndex == -1) {
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
      this.list_.scrollTop = 0;
    }
  }

  /**
   * Called when an element in the table is focused. Marks the table as having
   * a focused element, and dispatches an event if it didn't have focus.
   * @param {Event} e The focus event.
   * @private
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleElementFocus_(e) {
    if (!this.hasElementFocus) {
      this.hasElementFocus = true;
      // Force styles based on hasElementFocus to take effect.
      // @ts-ignore: error TS2551: Property 'list_' does not exist on type
      // 'Table'. Did you mean 'list'?
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
  // @ts-ignore: error TS6133: 'handleElementBlur_' is declared but its value is
  // never read.
  handleElementBlur_(e) {
    // When the blur event happens we do not know who is getting focus so we
    // delay this a bit until we know if the new focus node is outside the
    // table.
    const table = this;
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    const list = this.list_;
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'EventTarget'.
    const doc = e.target.ownerDocument;
    window.setTimeout(function() {
      const activeElement = doc.activeElement;
      // @ts-ignore: error TS2339: Property 'contains' does not exist on type
      // 'Table'.
      if (!table.contains(activeElement)) {
        table.hasElementFocus = false;
        // Force styles based on hasElementFocus to take effect.
        // @ts-ignore: error TS18047: 'list' is possibly 'null'.
        list.redraw();
      }
    }, 0);
  }

  /**
   * Adjust column width to fit its content.
   * @param {number} index Index of the column to adjust width.
   */
  fitColumn(index) {
    // @ts-ignore: error TS2551: Property 'list_' does not exist on type
    // 'Table'. Did you mean 'list'?
    const list = this.list_;
    // @ts-ignore: error TS18047: 'list' is possibly 'null'.
    const listHeight = list.clientHeight;

    const cm = this.columnModel_;
    const dm = this.dataModel;
    const columnId = cm.getId(index);
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'Table'.
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
      // @ts-ignore: error TS18047: 'list' is possibly 'null'.
      const items = list.getItemsInViewPort(list.scrollTop, listHeight);
      const firstIndex = Math.floor(Math.max(
          0, (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
      const lastIndex =
          Math.min(dm.length, firstIndex + MAXIMUM_ROWS_TO_MEASURE);
      for (let i = firstIndex; i < lastIndex; i++) {
        const item = dm.item(i);
        const div = doc.createElement('div');
        div.className = 'table-row-cell';
        // @ts-ignore: error TS2345: Argument of type 'this' is not assignable
        // to parameter of type 'Element'.
        div.appendChild(render(item, columnId, table));
        container.appendChild(div);
      }
      // @ts-ignore: error TS18047: 'list' is possibly 'null'.
      list.appendChild(container);
      const width = parseFloat(window.getComputedStyle(container).width);
      // @ts-ignore: error TS18047: 'list' is possibly 'null'.
      list.removeChild(container);
      cm.setWidth(index, width);
    });
  }

  normalizeColumns() {
    // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on type
    // 'Table'.
    this.columnModel.normalizeWidths(this.clientWidth);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
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

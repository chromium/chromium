// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column representation
 */

import {dispatchPropertyChange, getPropertyDescriptor} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

/**
 * A table column that wraps column ids and settings.
 */
export class TableColumn extends EventTarget {
  /**
   * @param {string} id
   * @param {string} name
   * @param {number} width
   * @param {boolean=} opt_endAlign
   */
  constructor(id, name, width, opt_endAlign) {
    super();

    this.id_ = id;
    this.name_ = name;
    this.width_ = width;
    this.endAlign_ = !!opt_endAlign;
    this.visible_ = true;
    this.defaultOrder_ = 'asc';
  }

  /**
   * Clones column.
   * @return {TableColumn} Clone of the given column.
   */
  clone() {
    const tableColumn =
        new TableColumn(this.id_, this.name_, this.width_, this.endAlign_);
    tableColumn.renderFunction = this.renderFunction_;
    tableColumn.headerRenderFunction = this.headerRenderFunction_;
    tableColumn.defaultOrder = this.defaultOrder_;

    tableColumn.visible_ = this.visible_;

    return tableColumn;
  }

  /**
   * Renders table cell. This is the default render function.
   * @param {*} dataItem The data item to be rendered.
   * @param {string} columnId The column id.
   * @param {Element} table The table.
   * @return {HTMLElement} Rendered element.
   */
  renderFunction_(dataItem, columnId, table) {
    const div = /** @type {HTMLElement} */
        (table.ownerDocument.createElement('div'));
    div.textContent = dataItem[columnId];
    div.hidden = !this.visible;
    return div;
  }

  /**
   * Renders table header. This is the default render function.
   * @param {Element} table The table.
   * @return {Text} Rendered text node.
   */
  headerRenderFunction_(table) {
    return table.ownerDocument.createTextNode(this.name);
  }

  /**
   * The width of the column.  Hidden columns have zero width.
   * @type {number}
   */
  get width() {
    return this.visible_ ? this.width_ : 0;
  }

  set width(value) {
    const oldValue = this.width;
    if (value !== oldValue) {
      this.width_ = value;
      dispatchPropertyChange(this, 'width', value, oldValue);
    }
  }

  /**
   * The width of the column, disregarding visibility.  For hidden columns,
   * this would be the width of the column if it were to be made visible.
   * @type {number}
   */
  get absoluteWidth() {
    return this.width_;
  }
}

  /**
   * The column id.
   * @type {string}
   */
// @ts-ignore: error TS2565: Property 'id' is used before being assigned.
TableColumn.prototype.id;
Object.defineProperty(TableColumn.prototype, 'id', getPropertyDescriptor('id'));

/**
 * The column name
 * @type {string}
 */
// @ts-ignore: error TS2565: Property 'name' is used before being assigned.
TableColumn.prototype.name;
Object.defineProperty(
    TableColumn.prototype, 'name', getPropertyDescriptor('name'));

/**
 * The column visibility.
 * @type {boolean}
 */
// @ts-ignore: error TS2565: Property 'visible' is used before being assigned.
TableColumn.prototype.visible;
Object.defineProperty(
    TableColumn.prototype, 'visible', getPropertyDescriptor('visible'));

/**
 * True if the column is aligned to end.
 * @type {boolean}
 */
// @ts-ignore: error TS2565: Property 'endAlign' is used before being assigned.
TableColumn.prototype.endAlign;
Object.defineProperty(
    TableColumn.prototype, 'endAlign', getPropertyDescriptor('endAlign'));

/**
 * The column render function.
 * @type {function(*, string, Element): HTMLElement}
 */
// @ts-ignore: error TS2565: Property 'renderFunction' is used before being
// assigned.
TableColumn.prototype.renderFunction;
Object.defineProperty(
    TableColumn.prototype, 'renderFunction',
    getPropertyDescriptor('renderFunction'));

/**
 * The column header render function.
 * @type {function(Element):Node}
 */
// @ts-ignore: error TS2565: Property 'headerRenderFunction' is used before
// being assigned.
TableColumn.prototype.headerRenderFunction;
Object.defineProperty(
    TableColumn.prototype, 'headerRenderFunction',
    getPropertyDescriptor('headerRenderFunction'));

/**
 * Default sorting order for the column ('asc' or 'desc').
 * @type {string}
 */
// @ts-ignore: error TS2565: Property 'defaultOrder' is used before being
// assigned.
TableColumn.prototype.defaultOrder;
Object.defineProperty(
    TableColumn.prototype, 'defaultOrder',
    getPropertyDescriptor('defaultOrder'));

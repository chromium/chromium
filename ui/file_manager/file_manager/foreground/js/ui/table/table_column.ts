// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column representation
 */

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {jsSetter} from '../../../../common/js/cr_ui.js';

import type {Table} from './table.js';

export type RenderFunction = (_1: any, _2: string, _3: Table) => HTMLElement;

export type SortDirection = 'asc'|'desc';
/**
 * A table column that wraps column ids and settings.
 */
export class TableColumn extends EventTarget {
  private visible_: boolean = true;
  private defaultOrder_: SortDirection = 'asc';

  constructor(
      private id_: string, private name_: string, private width_: number,
      private endAlign_?: boolean) {
    super();
  }

  /**
   * Clones column.
   * @return Clone of the given column.
   */
  clone(): TableColumn {
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
   * @param dataItem The data item to be rendered.
   * @param columnId The column id.
   * @param table The table.
   * @return Rendered element.
   */
  private renderFunction_(dataItem: any, columnId: string, table: Element):
      HTMLElement {
    const div = table.ownerDocument.createElement('div');
    div.textContent = dataItem[columnId];
    div.hidden = !this.visible;
    return div;
  }

  /**
   * Renders table header. This is the default render function.
   * @param table The table.
   * @return Rendered text node.
   */
  private headerRenderFunction_(table: Element): Text {
    return table.ownerDocument.createTextNode(this.name);
  }

  /**
   * The width of the column.  Hidden columns have zero width.
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
   */
  get absoluteWidth() {
    return this.width_;
  }

  get id(): string {
    return this.id_;
  }

  set id(value: string) {
    jsSetter(this, 'id', value);
  }

  get name(): string {
    return this.name_;
  }

  set name(value: string) {
    jsSetter(this, 'name', value);
  }

  get visible(): boolean {
    return this.visible_;
  }

  set visible(value: boolean) {
    jsSetter(this, 'visible', value);
  }

  get endAlign(): boolean {
    return !!this.endAlign_;
  }

  set endAlign(value: boolean) {
    jsSetter(this, 'endAlign', value);
  }

  get renderFunction(): RenderFunction {
    return this.renderFunction_;
  }

  set renderFunction(value: RenderFunction) {
    jsSetter(this, 'renderFunction', value);
  }

  get headerRenderFunction(): Function {
    return this.headerRenderFunction_;
  }

  set headerRenderFunction(value: Function) {
    jsSetter(this, 'headerRenderFunction', value);
  }

  get defaultOrder(): string {
    return this.defaultOrder_;
  }

  set defaultOrder(value: string) {
    jsSetter(this, 'defaultOrder', value);
  }
}

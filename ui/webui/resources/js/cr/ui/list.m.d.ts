// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from './array_data_model.m.js';
import {ListItem} from './list_item.m.js';
import {ListSelectionController} from './list_selection_controller.m.js';
import {ListSelectionModel} from './list_selection_model.m.js';

export interface Size {
  height: number;
  marginBottom: number;
  marginLeft: number;
  marginRight: number;
  marginTop: number;
  width: number;
}

declare class List extends HTMLUListElement {
  constructor();
  disabled: boolean;
  hasElementFocus: boolean;
  get itemConstructor(): Function;
  set itemConstructor(func: Function);
  set dataModel(dataModel: ArrayDataModel);
  get dataModel(): ArrayDataModel;
  get selectionModel(): ListSelectionModel;
  set selectionModel(sm: ListSelectionModel);
  get autoExpands(): boolean;
  set autoExpands(autoExpands: boolean);
  get fixedHeight(): boolean;
  set fixedHeight(fixedHeight: boolean);
  get selectedItem(): any;
  set selectedItem(selectedItem: any);
  get selectedItems(): any[];
  get items(): HTMLCollection;
  isItem(child: Node): boolean;
  startBatchUpdates(): void;
  endBatchUpdates(): void;
  decorate(): void;
  measureItem(item?: ListItem): Size|undefined;
  getListItemAncestor(element?: HTMLElement): HTMLElement|undefined;
  handleKeyDown(e: Event): void;
  handleScroll(e: Event): void;
  getItemTop(index: number): number;
  getItemRow(index: number): number;
  getFirstItemInRow(row: number): number;
  scrollIndexIntoView(index: number): void;
  getRectForContextMenu(): ClientRect;
  getListItem(value: any): ListItem;
  getListItemByIndex(index: number): ListItem;
  getIndexOfListItem(item: HTMLLIElement): number;
  createItem(value?: any): ListItem;
  createSelectionController(sm: ListSelectionModel): ListSelectionController;
  getHeightsForIndex(index: number): {top: number, height: number};
  getItemsInViewPort(scrollTop: number, clientHeight: number):
      {first: number, length: number, last: number};
  mergeItems(firstIndex: number, lastIndex: number): void;
  ensureAllItemSizesInCache(): void;
  getAfterFillerHeight(lastIndex: number): number;
  redraw(): void;
  restoreLeadItem(leadItem: ListItem): void;
  invalidate(): void;
  redrawItem(index: number): void;
  activateItemAtIndex(index: number): void;
  ensureLeadItemExists(): ListItem;
  startDragSelection(event: Event): void;
}

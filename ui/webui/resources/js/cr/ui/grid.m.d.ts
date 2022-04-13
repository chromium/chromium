// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {List} from './list.m.js';
import {ListSelectionController} from './list_selection_controller.m.js';
import {ListSelectionModel} from './list_selection_model.m.js';

declare class GridItem extends HTMLLIElement {
  constructor(dataItem: any);
  decorate(...args: any[]): void;
  textContent: any;
}

declare class GridSelectionController extends ListSelectionController {
  constructor(selectionModel: ListSelectionModel, grid: Grid);
  getIndexBelow(index: number): number;
  getIndexAbove(index: number): number;
  getIndexBefore(index: number): number;
  getIndexAfter(index: number): number;
  getNextIndex(index: number): number;
  getPreviousIndex(index: number): number;
  getFirstIndex(): number;
  getLastIndex(): number;
  handlePointerDownUp(e: Event, index: number): void;
  handleTouchEvents(e: Event, index: number): void;
  handleKeyDown(e: Event): void;
  isAccessibilityEnabled(): boolean;
}

declare class Grid extends List {
  get fixedHeight(): boolean;
  set fixedHeight(fixedHeight: boolean);
  get columns(): number;
  set columns(value: number);

  getItemTop(index: number): number;
  getItemRow(index: number): number;

  getFirstItemInRow(row: number): number;
  createSelectionController(sm: ListSelectionModel): ListSelectionController;
  getItemsInViewPort(scrollTop: number, clientHeight: number):
      {first: number, length: number, last: number};
  mergeItems(firstIndex: number, lastIndex: number): void;
  getAfterFillerHeight(lastIndex: number): number;
  isItem(child: Node): boolean;
  redraw(): void;
}

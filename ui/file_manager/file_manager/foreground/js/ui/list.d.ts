// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {ListSelectionModel} from './list_selection_model.js';

export class List extends HTMLUListElement {
  itemConstructor: new(...args: any) => ListItem;
  dataModel: ArrayDataModel;
  selectionModel: ListSelectionModel;
  handleSelectedSuggestion(selectedObject: any): void;
  getListItemByIndex: (index: number) => ListItem | null;
  getListItemAncestor: (element: Element) => ListItem;
  shouldStartDragSelection: (event: MouseEvent) => boolean;
  hasDragHitElement: (event: MouseEvent) => boolean;
  isItem: (item: Node) => boolean;
}

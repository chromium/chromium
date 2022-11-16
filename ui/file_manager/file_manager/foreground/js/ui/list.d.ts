// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {ListItem} from './list_item.d.js';

export class List extends HTMLUListElement {
  itemConstructor: new(...args: any) => ListItem;
  dataModel: ArrayDataModel;
  handleSelectedSuggestion(selectedObject: any): void;
}

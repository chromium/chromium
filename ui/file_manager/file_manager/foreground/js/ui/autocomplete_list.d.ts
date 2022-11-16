// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {List} from './list.js';

export class AutocompleteList extends List {
  suggestions: any[];
  syncWidthAndPositionToInput: () => void;
  handleEnterKeydown: () => void;
  attachToInput: (element: Element) => void;
  detach: () => void;
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class ListItem extends HTMLLIElement {
  // Plain text label.
  label: string;

  // Whether the item is teh lead in a selection.
  lead: boolean;

  // This item's index in teh containing list.
  listIndex: number;

  // Whether this item is selected.
  selected: boolean;

  // Called when an element is decorated as a list item.
  decorate: () => void;

  // Called when the selection state of this element changes.
  selectionChanged: () => void;
}

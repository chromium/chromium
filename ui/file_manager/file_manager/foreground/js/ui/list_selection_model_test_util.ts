// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {ListSelectionModel} from './list_selection_model.js';
import type {ListSingleSelectionModel} from './list_single_selection_model.js';
// clang-format on

/**
 * Creates an array spanning a range of integer values.
 * @param start The first number in the range.
 * @param end The last number in the range inclusive.
 */
export function range(start: number, end: number): number[] {
  const a = [];
  for (let i = start; i <= end; i++) {
    a.push(i);
  }
  return a;
}

/**
 * Modifies a selection model.
 * @param model The selection model to adjust.
 * @param index Starting index of the edit.
 * @param removed Number of entries to remove from the list.
 * @param added Number of entries to add to the list.
 */
export function adjust(
    model: ListSelectionModel|ListSingleSelectionModel, index: number,
    removed: number, added: number) {
  const permutation = [];
  for (let i = 0; i < index; i++) {
    permutation.push(i);
  }
  for (let j = 0; j < removed; j++) {
    permutation.push(-1);
  }
  for (let k = index + removed; k < model.length; k++) {
    permutation.push(k - removed + added);
  }
  model.adjustLength(model.length - removed + added);
  model.adjustToReordering(permutation);
}

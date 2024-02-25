// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {ListSelectionModel, type SelectionChangeEvent} from './list_selection_model.js';

import {assertArrayEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {assert} from 'chrome://resources/js/assert.js';

import {adjust, range} from './list_selection_model_test_util.js';
// clang-format on

/**
 * @param len size of the selection model.
 * @param dependentLeadItem inverse value for `independentLeadItem`
 *     defaults to true.
 */
function createSelectionModel(
    len: number, dependentLeadItem?: boolean): ListSelectionModel {
  const sm = new ListSelectionModel(len);
  sm['independentLeadItem'] = !dependentLeadItem;
  return sm;
}

export function testAdjust1() {
  const sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 10, 0);

  assertEquals(90, sm.leadIndex);
  assertEquals(90, sm.anchorIndex);
  assertEquals(90, sm.selectedIndex);
}

export function testAdjust2() {
  const sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 50;
  adjust(sm, 60, 10, 0);

  assertEquals(50, sm.leadIndex);
  assertEquals(50, sm.anchorIndex);
  assertEquals(50, sm.selectedIndex);
}

export function testAdjust3() {
  const sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 0, 10);

  assertEquals(110, sm.leadIndex);
  assertEquals(110, sm.anchorIndex);
  assertEquals(110, sm.selectedIndex);
}

export function testAdjust4() {
  const sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = 100;
  sm.selectRange(100, 110);

  adjust(sm, 0, 10, 5);

  assertEquals(95, sm.leadIndex);
  assertEquals(95, sm.anchorIndex);
  assertArrayEquals(range(95, 105), sm.selectedIndexes);
}

export function testAdjust5() {
  const sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 99;

  adjust(sm, 99, 1, 0);

  assertEquals(98, sm.leadIndex, 'lead');
  assertEquals(98, sm.anchorIndex, 'anchor');
  assertArrayEquals([98], sm.selectedIndexes);
}

export function testAdjust6() {
  const sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = 105;
  sm.selectRange(100, 110);

  // Remove 100 - 105
  adjust(sm, 100, 5, 0);

  assertEquals(100, sm.leadIndex, 'lead');
  assertEquals(100, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(100, 105), sm.selectedIndexes);
}

export function testAdjust7() {
  const sm = createSelectionModel(1);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 0;

  adjust(sm, 0, 0, 10);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals([10], sm.selectedIndexes);
}

export function testAdjust8() {
  const sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = 50;
  sm.selectAll();

  adjust(sm, 10, 80, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 19), sm.selectedIndexes);
}

export function testAdjust9() {
  const sm = createSelectionModel(10);

  sm.leadIndex = sm.anchorIndex = 5;
  sm.selectAll();

  // Remove all
  adjust(sm, 0, 10, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

export function testAdjust10() {
  const sm = createSelectionModel(10);

  sm.leadIndex = sm.anchorIndex = 5;
  sm.selectAll();

  adjust(sm, 0, 10, 20);

  assertEquals(5, sm.leadIndex, 'lead');
  assertEquals(5, sm.anchorIndex, 'anchor');
  assertArrayEquals([5], sm.selectedIndexes);
}

export function testAdjust11() {
  const sm = createSelectionModel(20);

  sm.leadIndex = sm.anchorIndex = 10;
  sm.selectAll();

  adjust(sm, 5, 20, 10);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 4), sm.selectedIndexes);
}

export function testAdjust12() {
  const sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 10;

  adjust(sm, 5, 20, 10);

  assertEquals(0, sm.leadIndex, 'lead');
  assertEquals(0, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 4), sm.selectedIndexes);
}

export function testAdjust13() {
  const sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 15;

  adjust(sm, 5, 5, 0);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 14), sm.selectedIndexes);
}

export function testAdjust14() {
  const sm = createSelectionModel(5, true);

  sm.selectedIndexes = [2, 3];
  sm.leadIndex = sm.anchorIndex = 3;

  adjust(sm, 2, 2, 0);

  assertEquals(2, sm.leadIndex, 'lead');
  assertEquals(2, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(2, 2), sm.selectedIndexes);
}

export function testAdjust15() {
  const sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 1;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

export function testAdjust16() {
  const sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 3;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

export function testAdjust17() {
  const sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 5;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

export function testLeadAndAnchor1() {
  const sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 10;

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
}

export function testLeadAndAnchor2() {
  const sm = createSelectionModel(20, true);

  sm.leadIndex = sm.anchorIndex = 10;
  sm.selectAll();

  assertEquals(0, sm.leadIndex, 'lead');
  assertEquals(0, sm.anchorIndex, 'anchor');
}

export function testSelectAll() {
  const sm = createSelectionModel(10);

  let changes: SelectionChangeEvent['detail']['changes']|null = null;
  sm.addEventListener('change', (e) => {
    changes = e.detail.changes;
  });

  sm.selectAll();

  assert(changes);
  assertArrayEquals(range(0, 9), sm.selectedIndexes);
  assertArrayEquals(
      range(0, 9),
      (changes as SelectionChangeEvent['detail']['changes']).map((change) => {
        return change.index;
      }));
}

export function testSelectAllOnEmptyList() {
  const sm = createSelectionModel(0);

  let changes: SelectionChangeEvent['detail']['changes']|null = null;
  sm.addEventListener('change', (e) => {
    changes = e.detail.changes;
  });

  sm.selectAll();

  assertArrayEquals([], sm.selectedIndexes);
  assertEquals(null, changes);
}

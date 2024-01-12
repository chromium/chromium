// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ArrayDataModel, type ChangeEvent, type PermutationEvent, type SpliceEvent} from './array_data_model.js';

export function testSlice() {
  const m = new ArrayDataModel([0, 1, 2]);
  assertArrayEquals([0, 1, 2], m.slice());
  assertArrayEquals([1, 2], m.slice(1));
  assertArrayEquals([1], m.slice(1, 2));
}

export function testPush() {
  const m = new ArrayDataModel([0, 1, 2]);

  let count = 0;
  m.addEventListener('splice', (e: SpliceEvent) => {
    count++;
    assertEquals(3, e.detail.index);
    assertArrayEquals([], e.detail.removed);
    assertArrayEquals([3, 4], e.detail.added);
  });

  assertEquals(5, m.push(3, 4));
  const a = m.slice();
  assertArrayEquals([0, 1, 2, 3, 4], a);

  assertEquals(1, count, 'The splice event should only fire once');
}

type SpliceFunctionArgs =
    [start: number, deleteCount: number, ...items: number[]];

export function testSplice() {
  function compare(array: number[], args: SpliceFunctionArgs) {
    const m = new ArrayDataModel<number>(array.slice());
    const expected = array.slice();
    const result = expected.splice(...args);
    assertArrayEquals(result, m.splice(...args));
    assertArrayEquals(expected, m.slice());
  }

  compare([1, 2, 3], [] as unknown as SpliceFunctionArgs);
  compare([1, 2, 3], [0, 0]);
  compare([1, 2, 3], [0, 1]);
  compare([1, 2, 3], [1, 1]);
  compare([1, 2, 3], [0, 3]);
  compare([1, 2, 3], [0, 1, 5]);
  compare([1, 2, 3], [0, 3, 1, 2, 3]);
  compare([1, 2, 3], [5, 3, 1, 2, 3]);
}

export function testPermutation() {
  function doTest(sourceArray: number[], spliceArgs: SpliceFunctionArgs) {
    const m = new ArrayDataModel<number>(sourceArray.slice());
    let permutation: number[] = [];
    m.addEventListener('permuted', (event: PermutationEvent) => {
      permutation = event.detail.permutation;
    });
    m.splice(...spliceArgs);
    let deleted = 0;
    for (let i = 0; i < sourceArray.length; i++) {
      if (permutation[i] === -1) {
        deleted++;
      } else {
        assertEquals(sourceArray[i], m.item(permutation[i]!));
      }
    }
    assertEquals(deleted, spliceArgs[1]);
  }

  doTest([1, 2, 3], [0, 0]);
  doTest([1, 2, 3], [0, 1]);
  doTest([1, 2, 3], [1, 1]);
  doTest([1, 2, 3], [0, 3]);
  doTest([1, 2, 3], [0, 1, 5]);
  doTest([1, 2, 3], [0, 3, 1, 2, 3]);
}

export function testUpdateIndexes() {
  const m = new ArrayDataModel<number>([1, 2, 3]);
  const changedIndexes: number[] = [];
  m.addEventListener('change', (event: ChangeEvent) => {
    changedIndexes.push(event.detail.index);
  });
  m.updateIndexes([0, 1, 2]);
  assertArrayEquals([0, 1, 2], changedIndexes);
}

export function testReplaceItem() {
  const m = new ArrayDataModel<number>([1, 2, 3]);
  let permutation = null;
  let changeIndex;
  m.addEventListener('permuted', (event: PermutationEvent) => {
    permutation = event.detail.permutation;
  });
  m.addEventListener('change', (event: ChangeEvent) => {
    changeIndex = event.detail.index;
  });
  m.replaceItem(2, 4);
  assertEquals(null, permutation);
  assertEquals(1, changeIndex);
}

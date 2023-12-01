// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {ArrayDataModel} from './array_data_model.js';

import {assertArrayEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
// clang-format on

export function testSlice() {
  const m = new ArrayDataModel([0, 1, 2]);
  assertArrayEquals([0, 1, 2], m.slice());
  assertArrayEquals([1, 2], m.slice(1));
  assertArrayEquals([1], m.slice(1, 2));
}

export function testPush() {
  const m = new ArrayDataModel([0, 1, 2]);

  let count = 0;
  m.addEventListener('splice', function(e) {
    const spliceEventDetail =
        /**
     @type {import('../../definitions/array_data_model_events.js').ArrayDataModelSpliceEvent}
       */
        (e).detail;
    count++;
    assertEquals(3, spliceEventDetail.index);
    assertArrayEquals([], spliceEventDetail.removed);
    assertArrayEquals([3, 4], spliceEventDetail.added);
  });

  assertEquals(5, m.push(3, 4));
  const a = m.slice();
  assertArrayEquals([0, 1, 2, 3, 4], a);

  assertEquals(1, count, 'The splice event should only fire once');
}

export function testSplice() {
  // @ts-ignore: error TS7006: Parameter 'args' implicitly has an 'any' type.
  function compare(array, args) {
    const m = new ArrayDataModel(array.slice());
    const expected = array.slice();
    const result = expected.splice.apply(expected, args);
    assertArrayEquals(result, m.splice.apply(m, args));
    assertArrayEquals(expected, m.slice());
  }

  compare([1, 2, 3], []);
  compare([1, 2, 3], [0, 0]);
  compare([1, 2, 3], [0, 1]);
  compare([1, 2, 3], [1, 1]);
  compare([1, 2, 3], [0, 3]);
  compare([1, 2, 3], [0, 1, 5]);
  compare([1, 2, 3], [0, 3, 1, 2, 3]);
  compare([1, 2, 3], [5, 3, 1, 2, 3]);
}

export function testPermutation() {
  // @ts-ignore: error TS7006: Parameter 'spliceArgs' implicitly has an 'any'
  // type.
  function doTest(sourceArray, spliceArgs) {
    const m = new ArrayDataModel(sourceArray.slice());
    let permutation;
    m.addEventListener('permuted', function(event) {
      // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
      // 'Event'.
      permutation = event.permutation;
    });
    m.splice.apply(m, spliceArgs);
    let deleted = 0;
    for (let i = 0; i < sourceArray.length; i++) {
      // @ts-ignore: error TS18048: 'permutation' is possibly 'undefined'.
      if (permutation[i] === -1) {
        deleted++;
      } else {
        // @ts-ignore: error TS18048: 'permutation' is possibly 'undefined'.
        assertEquals(sourceArray[i], m.item(permutation[i]));
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
  const m = new ArrayDataModel([1, 2, 3]);
  // @ts-ignore: error TS7034: Variable 'changedIndexes' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const changedIndexes = [];
  m.addEventListener('change', function(event) {
    const
        spliceEvent = /**
                         @type {import('../../definitions/array_data_model_events.js').ArrayDataModelSpliceEvent}
                           */
        (event);
    changedIndexes.push(spliceEvent.detail.index);
  });
  m.updateIndexes([0, 1, 2]);
  // @ts-ignore: error TS7005: Variable 'changedIndexes' implicitly has an
  // 'any[]' type.
  assertArrayEquals([0, 1, 2], changedIndexes);
}

export function testReplaceItem() {
  const m = new ArrayDataModel([1, 2, 3]);
  let permutation = null;
  let changeIndex;
  m.addEventListener('permuted', function(event) {
    // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
    // 'Event'.
    permutation = event.permutation;
  });
  m.addEventListener('change', function(event) {
    const
        spliceEvent = /**
                         @type {import('../../definitions/array_data_model_events.js').ArrayDataModelSpliceEvent}
                           */
        (event);
    changeIndex = spliceEvent.detail.index;
  });
  m.replaceItem(2, 4);
  assertEquals(null, permutation);
  assertEquals(1, changeIndex);
}

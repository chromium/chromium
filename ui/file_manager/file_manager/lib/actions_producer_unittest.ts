// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import {keepLatest} from './concurrency_models.js';
import {actionsProducerSuccess, TestAction} from './for_tests.js';

/**
 * Helper to accumulate all produced actions from the ActionsProducer.
 */
async function consumeGenerator(gen: AsyncGenerator, producedResults: any[]) {
  while (true) {
    const {value, done} = await gen.next();
    if (value) {
      producedResults.push(value);
    }
    if (done) {
      break;
    }
  }
}

/**
 * Tests the keepLatest() concurrency model. Checks that the latest call
 * overtakes any previous call.
 */
export async function testKeepLatest(done: () => void) {
  // keepLatest() wraps any ActionsGenerator.
  const action = keepLatest(actionsProducerSuccess);

  // Array to collect all the generated actions.
  const results: TestAction[] = [];

  // `first` here is a generator,  an ActionsProducer, wrapped in the
  // concurrency model `keepLatest`.
  const first = action('first-action');

  // Starts consuming the generated actions and can start another one in
  // parallel.
  const {value} = await first.next();
  assertEquals(value!.type, 'step#0');
  assertEquals(value!.payload, 'first-action');

  // A new call to the `action` will cause the previous `first` to be cancelled.
  const second = action('second-action');
  const secondValue = await second.next();
  assertEquals(secondValue.value!.type, 'step#0');
  assertEquals(secondValue.value!.payload, 'second-action');
  results.push(secondValue.value!);

  // At this point `first` should be cancelled by the keepLatest().
  // However, the exception doesn't show up here, because the exception is in
  // the actionsProducerSuccess() and isn't surfaced by the keepLast().
  const result = await first.next();
  assertEquals(result.value, undefined);
  assertEquals(result.done, true);

  // Await for the second generator to be fully consumed.
  await consumeGenerator(second, results);

  // The second action generates 4 results: 1 at start, 2 from the args [2, 2]
  // and 1 for the final action.
  await waitUntil(
      () => results.filter(r => r.payload === 'second-action').length === 4);

  done();
}

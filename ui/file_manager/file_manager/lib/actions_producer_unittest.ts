// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../common/js/mock_chrome.js';
import {waitUntil} from '../common/js/test_error_reporting.js';

import {ConcurrentActionInvalidatedError} from './actions_producer.js';
import type {Action} from './base_store.js';
import {keepLatest, keyedKeepFirst, keyedKeepLatest} from './concurrency_models.js';
import {setupTestStore} from './for_tests.js';

export function setUp() {
  installMockChrome({});
}
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
export async function testKeepLatest() {
  const {actionsProducerSuccess} = setupTestStore();

  // keepLatest() wraps any ActionsGenerator.
  const action = keepLatest(actionsProducerSuccess);

  // Array to collect all the generated actions.
  const results: Action[] = [];

  // `first` here is a generator,  an ActionsProducer, wrapped in the
  // concurrency model `keepLatest`.
  const first = action('first-action');

  // Starts consuming the generated actions and can start another one in
  // parallel.
  const {value} = (await first.next());
  assertEquals(value!.type, '[test] step');
  assertEquals(value!.payload, '0 first-action');

  // A new call to the `action` will cause the previous `first` to be cancelled.
  const second = action('second-action');
  const secondValue = await second.next();
  assertEquals(secondValue.value!.type, '[test] step');
  assertEquals(secondValue.value!.payload, '0 second-action');
  results.push(secondValue.value!);

  // At this point `first` should be cancelled by the keepLatest().
  // However, the exception only shows up in the next iteration of the AP.
  try {
    await first.next();
    assertNotReached('`first` should have failed');
  } catch (error) {
    assertTrue(error instanceof ConcurrentActionInvalidatedError);
    // Check that the generator is in `done=true` state.
    const {value, done} = await first.next();
    assertEquals(value, undefined);
    assertEquals(done, true);
  }

  // Await for the second generator to be fully consumed.
  await consumeGenerator(second, results);

  // The second action generates 4 results: 1 at start, 2 from the args [2, 2]
  // and 1 for the final action.
  await waitUntil(
      () => results.filter(r => r.payload.includes('second-action')).length ===
          4);
}

/**
 * Tests the keyedKeepFirst() concurrency model. Checks that the first call
 * with the same key, doesn't start a new AP, and calls with a new key cancels
 * the previous APs and starts a new AP.
 */
export async function testKeyedKeepFirst() {
  const {actionsProducerSuccess} = setupTestStore();

  const action = keyedKeepFirst(actionsProducerSuccess, (payload: string) => {
    return `key-${payload}`;
  });

  // `first` here is a generator,  an ActionsProducer, wrapped in the
  // concurrency model `keyedKeepFirst`.
  const first = action('file-key-1');

  // Starts consuming the generated actions and can start another one in
  // parallel.
  const {value} = await first.next();
  assertEquals(value!.type, '[test] step');
  assertEquals(value!.payload, '0 file-key-1');

  // Make a second call with the same key, which finishes in the first iteration
  // without generating any action.
  const second = action('file-key-1');
  const secondValue = await second.next();
  assertEquals(secondValue!.value, undefined);
  assertEquals(secondValue!.done, true);

  // Start a new call with different key.
  // Array to collect all the generated actions.
  const results: Action[] = [];
  const third = action('another-key-2');
  const thirdValue = await third.next();
  assertEquals(thirdValue!.value!.type, '[test] step');
  assertEquals(thirdValue!.value!.payload, '0 another-key-2');
  results.push(thirdValue.value!);

  // At this point `first` should be cancelled by the keyedKeepFirst().
  // However, the exception only shows up in the next iteration of the AP.
  try {
    await first.next();
    assertNotReached('`first` should have failed');
  } catch (error) {
    assertTrue(error instanceof ConcurrentActionInvalidatedError);
    // Check that the generator is in `done=true` state.
    const {value, done} = await first.next();
    assertEquals(value, undefined);
    assertEquals(done, true);
  }

  // Await for the third generator to be fully consumed.
  await consumeGenerator(third, results);
  // The third action generates 4 results: 1 at start, 2 from the args [2,
  // 2] / and 1 for the final action.
  await waitUntil(
      () => results.filter(r => r.payload.includes('another-key-2')).length ===
          4);
}

/**
 * Tests the keyedKeepLatest() concurrency model. Checks that the first call
 * with the same key will cancel the pending AP with the same key, but shouldn't
 * affect the pending AP with different keys.
 */
export async function testKeyedKeepLatest() {
  const {actionsProducerSuccess} = setupTestStore();

  const action = keyedKeepLatest(actionsProducerSuccess, (payload: string) => {
    return `key-${payload}`;
  });

  // Array to collect all the generated actions.
  const results: Action[] = [];

  // `first` here is a generator, an ActionsProducer, wrapped in the
  // concurrency model `keyedKeepLatest`.
  const first = action('file-key-1');

  // Starts consuming the generated actions and can start another one in
  // parallel.
  const {value} = await first.next();
  assertEquals(value!.type, '[test] step');
  assertEquals(value!.payload, '0 file-key-1');

  // Make a second call with a different key, which shouldn't affect the first
  // action.
  const second = action('file-key-2');
  const secondValue = await second.next();
  assertEquals(secondValue.value!.type, '[test] step');
  assertEquals(secondValue.value!.payload, '0 file-key-2');
  results.push(secondValue.value!);

  // Start a new call with the same key as the first action, which cancels the
  // first action.
  const third = action('file-key-1');
  const thirdValue = await third.next();
  assertEquals(thirdValue.value!.type, '[test] step');
  assertEquals(thirdValue.value!.payload, '0 file-key-1');
  results.push(thirdValue.value!);

  // At this point `first` should be cancelled by the keyedKeepLatest().
  // However, the exception only shows up in the next iteration of the AP.
  try {
    await first.next();
    assertNotReached('`first` should have failed');
  } catch (error) {
    assertTrue(error instanceof ConcurrentActionInvalidatedError);
    // Check that the generator is in `done=true` state.
    const {value, done} = await first.next();
    assertEquals(value, undefined);
    assertEquals(done, true);
  }

  // Await for the second/third generator to be fully consumed.
  await Promise.all(
      [consumeGenerator(second, results), consumeGenerator(third, results)]);

  // The second/third action generates 4 results: 1 at start, 2 from the args
  // [2, 2] / and 1 for the final action.
  assertEquals(4, results.filter(r => r.payload.includes('file-key-1')).length);
  assertEquals(4, results.filter(r => r.payload.includes('file-key-2')).length);
}

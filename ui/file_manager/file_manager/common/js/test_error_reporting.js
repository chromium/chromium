// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that promise gets rejected.
 * @param {Promise<void>} promise
 */
export async function assertRejected(promise) {
  let triggeredError = false;
  try {
    await promise;
  } catch (e) {
    triggeredError = true;
  }

  if (!triggeredError) {
    throw new Error('Assertion Failed: Expected promise to be rejected');
  }
}

/**
 * Invokes a callback function depending on the result of promise.
 *
 * @param {Promise<void>} promise Promise.
 * @param {function(boolean):void} callback Callback function. True is passed if
 *     the test failed.
 */
export function reportPromise(promise, callback) {
  promise.then(
      () => {
        callback(/* error */ false);
      },
      (error) => {
        console.error(/** @type {!Error} */ (error).stack || error);
        callback(/* error */ true);
      });
}

/**
 * Waits until testFunction becomes true.
 * @param {function(): boolean} testFunction A function which is tested.
 * @return {!Promise<void>} A promise which is fulfilled when the testFunction
 *     becomes true.
 */
export async function waitUntil(testFunction) {
  while (!testFunction()) {
    await new Promise(resolve => setTimeout(resolve, 100));
  }
}

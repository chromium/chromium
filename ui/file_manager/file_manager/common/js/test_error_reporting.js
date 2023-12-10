// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that promise gets rejected.
 * @param {Promise<*>} promise
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

/** Logs after the testFunction has run >3x (100ms each sleep). */
const LOG_INTERVAL = 300;

/**
 * Waits until testFunction becomes true.
 * @param {function(): boolean} testFunction A function which is tested.
 * @return {!Promise<void>} A promise which is fulfilled when the testFunction
 *     becomes true.
 */
export async function waitUntil(testFunction) {
  const stack = new Error().stack;
  let logTime = Date.now() + LOG_INTERVAL;
  let logged = false;
  while (!testFunction()) {
    if (Date.now() > logTime) {
      console.warn(`>>> waitUntil():\nWaiting for ${
          testFunction.toString()}\n-----\nFrom: ${stack}`);
      logged = true;
      logTime += LOG_INTERVAL;
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  if (logged) {
    console.warn('<<< waitUntil() ended successfully.');
  }
}

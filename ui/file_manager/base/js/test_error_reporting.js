// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that promise gets rejected.
 * @param {Promise} promise
 */
async function assertRejected(promise) {
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
 * @param {Promise} promise Promise.
 * @param {function(boolean)} callback Callback function. True is passed if the
 *     test failed.
 */
function reportPromise(promise, callback) {
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
 * @return {!Promise} A promise which is fulfilled when the testFunction
 *     becomes true.
 */
function waitUntil(testFunction) {
  const INTERVAL_FOR_WAIT_UNTIL = 100;  // ms

  return new Promise((resolve) => {
    let tryTestFunction = () => {
      if (testFunction()) {
        resolve();
      } else {
        setTimeout(tryTestFunction, INTERVAL_FOR_WAIT_UNTIL);
      }
    };

    tryTestFunction();
  });
}

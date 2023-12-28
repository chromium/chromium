// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that promise gets rejected.
 */
export async function assertRejected(promise: Promise<any>) {
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

/** Logs after the testFunction has run >3x (100ms each sleep). */
const LOG_INTERVAL = 300;

/**
 * Waits until testFunction becomes true.
 * @param testFunction A function which is tested.
 * @return A promise which is fulfilled when the testFunction becomes true.
 */
export async function waitUntil(testFunction: () => boolean): Promise<void> {
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

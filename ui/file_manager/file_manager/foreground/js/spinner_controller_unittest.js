// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {Element}
 */
let spinner;

/**
 * @type {SpinnerController}
 */
let controller;

function waitForMutation(target) {
  return new Promise((fulfill, reject) => {
    const observer = new MutationObserver(mutations => {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

function setUp() {
  spinner = document.createElement('div');
  spinner.id = 'spinner';
  spinner.textContent = 'LOADING...';
  spinner.hidden = true;
  controller = new SpinnerController(assert(spinner));
  // Set the duration to 100ms, which is short enough, but also long enough
  // to happen later than 0ms timers used in test cases.
  controller.setBlinkDurationForTesting(100);
}

function testBlink(callback) {
  assertTrue(spinner.hidden);
  controller.blink();

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            return waitForMutation(spinner);
          })
          .then(() => {
            assertTrue(spinner.hidden);
          }),
      callback);
}

function testShow(callback) {
  assertTrue(spinner.hidden);
  const hideCallback = controller.show();

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);  // It should still be hidden.
            // Call asynchronously, so the mutation observer catches the change.
            setTimeout(hideCallback, 0);
            return waitForMutation(spinner);
          })
          .then(() => {
            assertTrue(spinner.hidden);
          }),
      callback);
}

function testShowDuringBlink(callback) {
  assertTrue(spinner.hidden);
  controller.blink();
  const hideCallback = controller.show();

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            hideCallback();
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            return waitForMutation(spinner);
          })
          .then(() => {
            assertTrue(spinner.hidden);
          }),
      callback);
}

function testStackedShows(callback) {
  assertTrue(spinner.hidden);

  const hideCallbacks = [];
  hideCallbacks.push(controller.show());
  hideCallbacks.push(controller.show());

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            hideCallbacks[1]();
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            // Call asynchronously, so the mutation observer catches the change.
            setTimeout(hideCallbacks[0], 0);
            return waitForMutation(spinner);
          })
          .then(() => {
            assertTrue(spinner.hidden);
          }),
      callback);
}

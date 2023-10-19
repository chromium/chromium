// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../common/js/test_error_reporting.js';

import {SpinnerController} from './spinner_controller.js';

/**
 * @type {HTMLElement}
 */
let spinner;

/**
 * @type {SpinnerController}
 */
let controller;

// @ts-ignore: error TS7006: Parameter 'target' implicitly has an 'any' type.
function waitForMutation(target) {
  // @ts-ignore: error TS6133: 'reject' is declared but its value is never read.
  return new Promise((fulfill, reject) => {
    // @ts-ignore: error TS6133: 'mutations' is declared but its value is never
    // read.
    const observer = new MutationObserver(mutations => {
      observer.disconnect();
      // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
      // Promise()' needs a JSDoc hint to produce a 'resolve' that can be called
      // without arguments.
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

export function setUp() {
  spinner = document.createElement('div');
  spinner.id = 'spinner';
  spinner.textContent = 'LOADING...';
  spinner.hidden = true;
  controller = new SpinnerController(assert(spinner));
  // Set the duration to 100ms, which is short enough, but also long enough
  // to happen later than 0ms timers used in test cases.
  controller.setBlinkDurationForTesting(100);
}

/** @param {()=>void} callback */
export function testBlink(callback) {
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

/** @param {()=>void} callback */
export function testShow(callback) {
  assertTrue(spinner.hidden);
  const hideCallback = controller.show();

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            // @ts-ignore: error TS6133: 'reject' is declared but its value is
            // never read.
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

/** @param {()=>void} callback */
export function testShowDuringBlink(callback) {
  assertTrue(spinner.hidden);
  controller.blink();
  const hideCallback = controller.show();

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            // @ts-ignore: error TS6133: 'reject' is declared but its value is
            // never read.
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            hideCallback();
            // @ts-ignore: error TS6133: 'reject' is declared but its value is
            // never read.
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

/** @param {()=>void} callback */
export function testStackedShows(callback) {
  assertTrue(spinner.hidden);

  // @ts-ignore: error TS7034: Variable 'hideCallbacks' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const hideCallbacks = [];
  hideCallbacks.push(controller.show());
  hideCallbacks.push(controller.show());

  return reportPromise(
      waitForMutation(spinner)
          .then(() => {
            assertFalse(spinner.hidden);
            // @ts-ignore: error TS6133: 'reject' is declared but its value is
            // never read.
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            // @ts-ignore: error TS7005: Variable 'hideCallbacks' implicitly has
            // an 'any[]' type.
            hideCallbacks[1]();
            // @ts-ignore: error TS6133: 'reject' is declared but its value is
            // never read.
            return new Promise((fulfill, reject) => {
              setTimeout(fulfill, 0);
            });
          })
          .then(() => {
            assertFalse(spinner.hidden);
            // Call asynchronously, so the mutation observer catches the change.
            // @ts-ignore: error TS7005: Variable 'hideCallbacks' implicitly has
            // an 'any[]' type.
            setTimeout(hideCallbacks[0], 0);
            return waitForMutation(spinner);
          })
          .then(() => {
            assertTrue(spinner.hidden);
          }),
      callback);
}

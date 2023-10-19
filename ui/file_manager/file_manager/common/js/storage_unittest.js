// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {storage} from './storage.js';
import {waitUntil} from './test_error_reporting.js';

/**
 * Used to store the test listener calls.
 */
let nsCalled = {};
let valuesChanged = {};

// @ts-ignore: error TS7006: Parameter 'namespace' implicitly has an 'any' type.
function listener(changedValues, namespace) {
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type 'any' can't be used to index type '{}'.
  nsCalled[namespace] = true;
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type 'any' can't be used to index type '{}'.
  valuesChanged[namespace] = changedValues;
}

export function setUp() {
  storage.onChanged.resetForTesting();

  // Add the test listeners to default onChanged which is a
  // StorageChangeTracker instance.
  storage.onChanged.addListener(listener);

  nsCalled = {};
  valuesChanged = {};
}

/**
 * Tests that changing the storage.local values is propagated to the
 * listeners of onChanged.
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testPropagateLocally(done) {
  // Check: That the listener receives the update.
  await storage.local.setAsync({'local key': 'local value'});
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type '"local"' can't be used to index type '{}'.
  await waitUntil(() => nsCalled['local']);
  assertDeepEquals(
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type '"local"' can't be used to index type '{}'.
      valuesChanged['local'], {'local key': {newValue: 'local value'}});

  done();
}

/**
 * Tests the value propagates in its original type, window.localStorage always
 * returns in string, we use JSON.parse() to convert back to original type.
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testNumberBool(done) {
  // Check: That the listener from `tracker` receives the update.
  await storage.local.setAsync({'number': 33, 'boolean': true});
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type '"local"' can't be used to index type '{}'.
  await waitUntil(() => nsCalled['local']);
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type '"local"' can't be used to index type '{}'.
  assertDeepEquals(valuesChanged['local'], {
    'number': {newValue: 33},
    'boolean': {newValue: true},
  });

  done();
}

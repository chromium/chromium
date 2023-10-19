// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertThrows} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../../common/js/test_error_reporting.js';

import {MetadataModel} from './metadata_model.js';
import {MetadataProvider} from './metadata_provider.js';

/** @final */
class TestMetadataProvider extends MetadataProvider {
  constructor() {
    super(['property', 'propertyA', 'propertyB']);
    this.requestCount = 0;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'requests' implicitly has an 'any'
  // type.
  get(requests) {
    this.requestCount++;
    // @ts-ignore: error TS7006: Parameter 'request' implicitly has an 'any'
    // type.
    return Promise.resolve(requests.map(request => {
      const entry = request.entry;
      const names = request.names;
      const result = {};
      for (let i = 0; i < names.length; i++) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'any' can't be used to index type '{}'.
        result[names[i]] = entry.toURL() + ':' + names[i];
      }
      return result;
    }));
  }
}

/** @final */
class TestEmptyMetadataProvider extends MetadataProvider {
  constructor() {
    super(['property']);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'requests' implicitly has an 'any'
  // type.
  get(requests) {
    return Promise.resolve(requests.map(() => {
      return {};
    }));
  }
}

/** @final */
class ManualTestMetadataProvider extends MetadataProvider {
  constructor() {
    super(['propertyA', 'propertyB', 'propertyC']);
    // @ts-ignore: error TS7008: Member 'callback' implicitly has an 'any[]'
    // type.
    this.callback = [];
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'requests' implicitly has an 'any'
  // type.
  get(requests) {
    return new Promise(fulfill => {
      this.callback.push(fulfill);
    });
  }
}

/** @type {!Entry} */
const entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
});

/** @type {!Entry} */
const entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
});

/**
 * Returns a property of a Metadata result object.
 * @param {Object} result Metadata result
 * @param {string} property Property name to return.
 * @return {string}
 */
function getProperty(result, property) {
  if (!result) {
    throw new Error('Fail: Metadata result is undefined');
  }
  // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
  // expression of type 'string' can't be used to index type 'Object'.
  return result[property];
}

/** @param {()=>void} callback */
export function testMetadataModelBasic(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['property']).then(results => {
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            'filesystem://A:property', getProperty(results[0], 'property'));
        assertEquals(
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            'filesystem://B:property', getProperty(results[1], 'property'));
      }),
      callback);
}

/** @param {()=>void} callback */
export function testMetadataModelRequestForCachedProperty(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['property'])
          .then(() => {
            // All the results should be cached here.
            return model.get([entryA, entryB], ['property']);
          })
          .then(results => {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            assertEquals(
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

export function testMetadataModelRequestForCachedAndNonCachedProperty(
    // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
    // type.
    callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['propertyA'])
          .then(() => {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            // propertyB has not been cached here.
            return model.get([entryA, entryB], ['propertyA', 'propertyB']);
          })
          .then(results => {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(2, provider.requestCount);
            assertEquals(
                'filesystem://A:propertyA',
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                getProperty(results[0], 'propertyA'));
            assertEquals(
                'filesystem://A:propertyB',
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                getProperty(results[0], 'propertyB'));
            assertEquals(
                'filesystem://B:propertyA',
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                getProperty(results[1], 'propertyA'));
            assertEquals(
                'filesystem://B:propertyB',
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                getProperty(results[1], 'propertyB'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testMetadataModelRequestForCachedAndNonCachedEntry(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA], ['property'])
          .then(() => {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            // entryB has not been cached here.
            return model.get([entryA, entryB], ['property']);
          })
          .then(results => {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(2, provider.requestCount);
            assertEquals(
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
                // undefined' is not assignable to parameter of type 'Object'.
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

export function testMetadataModelRequestBeforeCompletingPreviousRequest(
    // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
    // type.
    callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  model.get([entryA], ['property']);
  provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
  assertEquals(1, provider.requestCount);

  // The result of first call has not been fetched yet.
  reportPromise(
      model.get([entryA], ['property']).then(results => {
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            'filesystem://A:property', getProperty(results[0], 'property'));
      }),
      callback);
}

/** @param {()=>void} callback */
export function testMetadataModelNotUpdateCachedResultAfterRequest(callback) {
  let provider = new ManualTestMetadataProvider();
  const model = new MetadataModel(provider);

  const promise = model.get([entryA], ['propertyA']);
  provider = /** @type {!ManualTestMetadataProvider} */ (model.getProvider());
  provider.callback[0]([{propertyA: 'valueA1'}]);

  reportPromise(
      promise
          .then(() => {
            // 'propertyA' is cached here.
            const promise1 = model.get([entryA], ['propertyA', 'propertyB']);
            const promise2 = model.get([entryA], ['propertyC']);
            // Returns propertyC.
            provider = /** @type {!ManualTestMetadataProvider} */ (
                model.getProvider());
            provider.callback[2]([{propertyA: 'valueA2', propertyC: 'valueC'}]);
            provider.callback[1]([{propertyB: 'valueB'}]);
            return Promise.all([promise1, promise2]);
          })
          .then(results => {
            // The result should be cached value at the time when get was
            // called.
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            assertEquals('valueA1', getProperty(results[0][0], 'propertyA'));
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            assertEquals('valueB', getProperty(results[0][0], 'propertyB'));
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            assertEquals('valueC', getProperty(results[1][0], 'propertyC'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testMetadataModelGetCache(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  const promise = model.get([entryA], ['property']);
  const cache = model.getCache([entryA], ['property']);
  // @ts-ignore: error TS2345: Argument of type 'MetadataItem | undefined' is
  // not assignable to parameter of type 'Object'.
  assertEquals(null, getProperty(cache[0], 'property'));

  reportPromise(
      promise.then(() => {
        const cache = model.getCache([entryA], ['property']);
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            // @ts-ignore: error TS2345: Argument of type 'MetadataItem |
            // undefined' is not assignable to parameter of type 'Object'.
            'filesystem://A:property', getProperty(cache[0], 'property'));
      }),
      callback);
}

export function testMetadataModelUnknownProperty() {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  assertThrows(() => {
    model.get([entryA], ['unknown']);
  });
}

/** @param {()=>void} callback */
export function testMetadataModelEmptyResult(callback) {
  const provider = new TestEmptyMetadataProvider();
  const model = new MetadataModel(provider);

  // getImpl returns empty result.
  reportPromise(
      model.get([entryA], ['property']).then(results => {
        // @ts-ignore: error TS2345: Argument of type 'MetadataItem | undefined'
        // is not assignable to parameter of type 'Object'.
        assertEquals(undefined, getProperty(results[0], 'property'));
      }),
      callback);
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @final */
class TestMetadataProvider extends MetadataProvider {
  constructor() {
    super(['property', 'propertyA', 'propertyB']);
    this.requestCount = 0;
  }

  /** @override */
  get(requests) {
    this.requestCount++;
    return Promise.resolve(requests.map(request => {
      const entry = request.entry;
      const names = request.names;
      const result = {};
      for (let i = 0; i < names.length; i++) {
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
    this.callback = [];
  }

  /** @override */
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
  return result[property];
}

function testMetadataModelBasic(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['property']).then(results => {
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:property', getProperty(results[0], 'property'));
        assertEquals(
            'filesystem://B:property', getProperty(results[1], 'property'));
      }),
      callback);
}

function testMetadataModelRequestForCachedProperty(callback) {
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
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

function testMetadataModelRequestForCachedAndNonCachedProperty(callback) {
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
                getProperty(results[0], 'propertyA'));
            assertEquals(
                'filesystem://A:propertyB',
                getProperty(results[0], 'propertyB'));
            assertEquals(
                'filesystem://B:propertyA',
                getProperty(results[1], 'propertyA'));
            assertEquals(
                'filesystem://B:propertyB',
                getProperty(results[1], 'propertyB'));
          }),
      callback);
}

function testMetadataModelRequestForCachedAndNonCachedEntry(callback) {
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
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

function testMetadataModelRequestBeforeCompletingPreviousRequest(callback) {
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
            'filesystem://A:property', getProperty(results[0], 'property'));
      }),
      callback);
}

function testMetadataModelNotUpdateCachedResultAfterRequest(callback) {
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
            assertEquals('valueA1', getProperty(results[0][0], 'propertyA'));
            assertEquals('valueB', getProperty(results[0][0], 'propertyB'));
            assertEquals('valueC', getProperty(results[1][0], 'propertyC'));
          }),
      callback);
}

function testMetadataModelGetCache(callback) {
  let provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  const promise = model.get([entryA], ['property']);
  const cache = model.getCache([entryA], ['property']);
  assertEquals(null, getProperty(cache[0], 'property'));

  reportPromise(
      promise.then(() => {
        const cache = model.getCache([entryA], ['property']);
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:property', getProperty(cache[0], 'property'));
      }),
      callback);
}

function testMetadataModelUnknownProperty() {
  const provider = new TestMetadataProvider();
  const model = new MetadataModel(provider);

  assertThrows(() => {
    model.get([entryA], ['unknown']);
  });
}

function testMetadataModelEmptyResult(callback) {
  const provider = new TestEmptyMetadataProvider();
  const model = new MetadataModel(provider);

  // getImpl returns empty result.
  reportPromise(
      model.get([entryA], ['property']).then(results => {
        assertEquals(undefined, getProperty(results[0], 'property'));
      }),
      callback);
}

// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '../resources/indexeddb.html',
      'Tests that requestData can query both object stores and indices');

  await dp.IndexedDB.enable();
  await session.evaluateAsync('window.dbPromise');

  const {result: {objectStoreDataEntries}} = await dp.IndexedDB.requestData({
    securityOrigin: 'file://',
    databaseName: 'testDatabase1',
    objectStoreName: 'testStore1',
    skipCount: 0,
    pageSize: 10,
  });
  testRunner.log('Object store data entries');
  for (const {key, primaryKey, value} of objectStoreDataEntries) {
    testRunner.log(key, 'key');
    testRunner.log(primaryKey, 'primaryKey');
    testRunner.log(value, 'value');
  }

  const {result: {objectStoreDataEntries: indexDataEntries}} =
      await dp.IndexedDB.requestData({
        securityOrigin: 'file://',
        databaseName: 'testDatabase1',
        objectStoreName: 'testStore1',
        indexName: '',
        skipCount: 0,
        pageSize: 10,
      });
  testRunner.log('Index data entries');
  for (const {key, primaryKey, value} of indexDataEntries) {
    testRunner.log(key, 'key');
    testRunner.log(primaryKey, 'primaryKey');
    testRunner.log(value, 'value');
  }

  testRunner.completeTest();
});

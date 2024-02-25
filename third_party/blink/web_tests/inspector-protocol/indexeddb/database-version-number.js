// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/indexeddb.html',
      'Tests that IndexedDB database version is reported correctly');

  await dp.IndexedDB.enable();
  await session.evaluateAsync('window.dbPromise');

  const {result: {databaseWithObjectStores}} =
      await dp.IndexedDB.requestDatabase({
        databaseName: 'testDatabase1',
        securityOrigin: 'file://',
      });

  testRunner.log(databaseWithObjectStores);
  testRunner.completeTest();
});

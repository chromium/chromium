// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests RemoteObject.getProperties on localStorage object. 66215`);

  await session.evaluate(`localStorage.testProperty = "testPropertyValue";`);

  const {result: {result: localStorageObject}} =
      await dp.Runtime.evaluate({expression: 'localStorage'});
  const {result: {result: properties}} = await dp.Runtime.getProperties(
      {objectId: localStorageObject.objectId, ownProperties: true});

  for (const property of properties) {
    if (property.name === 'testProperty') {
      testRunner.log(property);
    }
  }

  await session.evaluate(`localStorage.removeItem('testProperty');`);
  testRunner.completeTest();
});

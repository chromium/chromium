// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.getProperties returns generator object details.');

  await dp.Runtime.enable();
  await session.evaluate(`
      function forward(iter, step)
      {
          while (step-- > 0)
              iter.next();
          return iter;
      }

      function* gen()
      {
          yield 1;
          yield 2;
          yield 3;
      }

      var obj = {
          generator: function*()
          {
              yield 11;
              yield 12;
              yield 13;
          }
      };

      var iterNotStarted = gen();
      var iterSuspended = forward(gen(), 1);
      var iterClosed = forward(gen(), 5);

      var iterObjGenerator = forward(obj.generator(), 2);

      var anonymousGenIter = (function*() {
          yield 21;
          yield 22;
          yield 23;
      })();
      forward(anonymousGenIter, 3);
  `);

  async function performStandardTestCase(pageExpression) {
    const {result: {result: object}} =
        await dp.Runtime.evaluate({expression: pageExpression});
    testRunner.log(`${pageExpression}: type = ${object.type}, subtype = ${
        object.subtype}`);

    const {result: response} = await dp.Runtime.getProperties({
      objectId: object.objectId,
      ownProperties: true,
      generatePreview: false,
    });
    if (!response?.internalProperties) {
      testRunner.log('FAIL: no properties');
      return;
    }

    for (const prop of response.internalProperties) {
      if (prop.name === '[[Prototype]]' || prop.name === '[[Scopes]]') {
        continue;
      }
      if (prop.name !== '[[GeneratorLocation]]') {
        const valueText = prop.value.description ?? prop.value.value;
        testRunner.log(`${prop.name} = ${valueText}`);
      } else {
        const location = prop.value.value;
        testRunner.log(`lineNumber = ${location.lineNumber}`);
        testRunner.log(`columnNumber = ${location.columnNumber}`);
        testRunner.log(`script is valid: ${location.scriptId ? 'yes' : 'no'}`);
      }
    }
  }

  testRunner.runTestSuite([
    async function testIterNotStarted() {
      await performStandardTestCase('iterNotStarted');
    },
    async function testIterSuspended() {
      await performStandardTestCase('iterSuspended');
    },
    async function testIterClosed() {
      await performStandardTestCase('iterClosed');
    },
    async function testIterObjGenerator() {
      await performStandardTestCase('iterObjGenerator');
    },
    async function testAnonymousGenIter() {
      await performStandardTestCase('anonymousGenIter');
    },
  ]);
});

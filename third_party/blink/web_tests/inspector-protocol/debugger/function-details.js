// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.getProperties returns accurate [[FunctionLocation]], [[IsGenerator]], and function name properties. Bug 71808');

  await dp.Debugger.enable();
  await session.evaluate(`${'\n'.repeat(10)}
        function firstLineFunction()

        {
        }

        function notFirstLineFunction()

        {
        }

        var obj = {
            m: function() {}
        }

        function functionWithDisplayName() {}
        functionWithDisplayName.displayName = "user-friendly name";

        function functionWithDisplayNameGetter() {}
        functionWithDisplayNameGetter.__defineGetter__("displayName", function() { return "FAIL_should_not_execute"; });

        var smallClosure = (function(p) { return function() { return p; }; })("Capybara");

        var bigClosure = (function(p) {
            var o = {
               e: 7,
               f: 5,
               get u() { return 3; },
               set v(value) { }
            };
            with (o) {
                try {
                    throw Error("Test");
                } catch (ex) {
                    return function() {
                        return String(p) + String(ex) + u + e;
                    };
                }
            }
        })({});

        function* gen() { yield [1,2,3] }
    `);

  function dumpFunctionDetails(properties) {
    const location = properties.get('[[FunctionLocation]]').value.value;
    testRunner.log('Function details: ');
    testRunner.log('lineNumber: ' + location.lineNumber);
    testRunner.log('columnNumber: ' + location.columnNumber);
    testRunner.log('scriptId is valid: ' + !!location.scriptId);
    testRunner.log('functionName: ' + properties.get('name').value.value);
    if (properties.has('[[IsGenerator]]')) {
      testRunner.log('isGenerator: ' +
                     properties.get('[[IsGenerator]]').value.value);
    }
  }

  async function performStandardTestCase(pageExpression) {
    const {result: {result: remote}} =
        await dp.Runtime.evaluate({expression: pageExpression});

    testRunner.log(pageExpression + ' type = ' + remote.type);
    const {result: response} = await dp.Runtime.getProperties(
        {objectId: remote.objectId, ownProperties: false});

    const propertiesMap = new Map();
    for (const prop of response.internalProperties) {
      propertiesMap.set(prop.name, prop);
    }
    for (const prop of response.result) {
      if (prop.name === 'name' && prop.value && prop.value.type === 'string') {
        propertiesMap.set('name', prop);
      }
      if (prop.name === 'displayName' && prop.value &&
          prop.value.type === 'string') {
        propertiesMap.set('name', prop);
        break;
      }
    }
    dumpFunctionDetails(propertiesMap);
  }

  testRunner.runTestSuite([
    async function testGetFirstLineFunctionDetails() {
      await performStandardTestCase('firstLineFunction');
    },
    async function testGetNonFirstLineFunctionDetails() {
      await performStandardTestCase('notFirstLineFunction');
    },
    async function testGetDetailsOfFunctionWithInferredName() {
      await performStandardTestCase('obj.m');
    },
    async function testGetDetailsOfFunctionWithDisplayName() {
      await performStandardTestCase('functionWithDisplayName');
    },
    async function testGetDetailsOfFunctionWithDisplayNameGetter() {
      await performStandardTestCase('functionWithDisplayNameGetter');
    },
    async function testSmallClosure() {
      await performStandardTestCase('smallClosure');
    },
    async function testBigClosure() {
      await performStandardTestCase('bigClosure');
    },
    async function testGenFunction() {
      await performStandardTestCase('gen');
    }
  ]);
});

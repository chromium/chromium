// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div onclick="void 0"></div>`,
      'Tests Runtime.evaluate, Runtime.consoleAPICalled, Runtime.getProperties, and getEventListeners with outer scope variables and polluted Object.prototype. Bug 60547.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const frame = params.stackTrace.callFrames[0];
    const text = params.args.map(a => a.value ?? a.description).join(' ');
    testRunner.log(`${frame.url || 'VM'}:${frame.lineNumber + 1} ${text}`);
  });

  await session.evaluate(`
    // Used to interfere into InjectedScript._propertyDescriptors()
    Object.prototype.get = function() { return "FAIL"; };
    Object.prototype.set = function() { return "FAIL"; };
    Object.prototype.value = "FAIL";
    Object.prototype.getter = "FAIL";
    Object.prototype.setter = "FAIL";
    Object.prototype.isOwn = true;
    // Used to interfere into InjectedScript.primitiveTypes
    Object.prototype.object = true;
    // Used to interfere into InjectedScript.getEventListeners()
    Object.prototype.nullValue = null;
    Object.prototype.undefValue = undefined;

    var foo = "bar";
    var testObj = {
      get getter() { },
      set setter(_) { },
      baz: "baz"
    };
  `);

  const snippet1 = `
    (function(obj) {
      with (obj) {
        console.log('with: ' + a);
        eval("console.log('eval in with: ' + a)");
      }
    })({ a: 'Object property value' });
    //# sourceURL=runtime-console-eval-scoped.js`;

  const snippet2 = `
    (function(a) {
      eval("console.log('eval in function: ' + a)");
    })('Function parameter');`;

  testRunner.runTestSuite([
    async function testSnippet1() {
      await session.evaluate(snippet1);
    },

    async function testSnippet2() {
      await session.evaluate(snippet2);
    },

    async function testConsoleEvalPrimitiveValue() {
      const {result} = await dp.Runtime.evaluate({
        expression: 'foo',
        objectGroup: 'console',
        includeCommandLineAPI: true,
        generatePreview: true,
        replMode: true,
      });
      testRunner.log(result);
    },

    async function testConsoleEvalObject() {
      const {result: {result}} =
          await dp.Runtime.evaluate({expression: 'testObj'});
      const {result: {result: properties}} = await dp.Runtime.getProperties({
        objectId: result.objectId,
        ownProperties: true,
      });
      for (const p of properties) {
        testRunner.log(p);
      }
    },

    async function testGetEventListenersDoesNotThrow() {
      const {result} = await dp.Runtime.evaluate({
        expression: 'getEventListeners(document.body.firstChild)',
        objectGroup: 'console',
        includeCommandLineAPI: true,
        generatePreview: true,
        replMode: true,
      });
      testRunner.log(result);
    },
  ]);
});

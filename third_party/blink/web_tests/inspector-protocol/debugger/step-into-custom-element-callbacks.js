// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that stepping into custom element methods will lead to a pause in the callbacks.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await dp.Runtime.evaluate({
    expression: `
      function output(message) {
        if (!self._output) {
          self._output = [];
        }
        self._output.push('[page] ' + message);
      }

      function testFunction() {
        class FooElement extends HTMLElement {
          constructor() {
            super();
            output('Invoked constructor.');
          }
          connectedCallback() {
            output('Invoked connectedCallback.');
          }
          disconnectedCallback() {
            output('Invoked disconnectedCallback.');
          }
          adoptedCallback() {
            output('Invoked adoptedCallback.');
          }
          attributeChangedCallback() {
            output('Invoked attributeChangedCallback.');
          }
          static get observedAttributes() {
            return ['x'];
          }
        }
        customElements.define('x-foo', FooElement);
        debugger;
        var foo = new FooElement();
        debugger;
        foo.setAttribute('x', 'b');
        debugger;
        document.body.appendChild(foo);
        debugger;
        foo.remove();
      }
    `,
    objectGroup: 'console',
  });

  function checkTopFrameFunction(callFrames, expectedName) {
    const topFunctionName = callFrames[0].functionName;
    if (expectedName === topFunctionName) {
      testRunner.log(`PASS: Did step into event listener(${expectedName}).`);
    } else {
      testRunner.log(`FAIL: Unexpected top function: expected ${
          expectedName}, found ${topFunctionName}`);
    }
  }

  async function stepOverThenIn(name) {
    testRunner.log(`Stepping to ${name}...`);
    const stepOverResumed = dp.Debugger.onceResumed();
    const stepOverPaused = dp.Debugger.oncePaused();
    await dp.Debugger.stepOver();
    await stepOverResumed;
    await stepOverPaused;

    testRunner.log(`Stepping into ${name}...`);
    const stepIntoResumed = dp.Debugger.onceResumed();
    const stepIntoPaused = dp.Debugger.oncePaused();
    await dp.Debugger.stepInto();
    await stepIntoResumed;
    const {params: {callFrames}} = await stepIntoPaused;
    return callFrames;
  }

  testRunner.log('Set timer for test function.');
  const initialPausedPromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate({
    expression: 'setTimeout(testFunction, 0)',
    objectGroup: 'console',
  });
  await initialPausedPromise;

  const steps = [
    ['constructor', 'FooElement'],
    ['setAttribute', 'attributeChangedCallback'],
    ['connectedCallback', 'connectedCallback'],
    ['disconnectedCallback', 'disconnectedCallback'],
  ];

  for (let i = 0; i < steps.length; ++i) {
    const [name, expectedFunctionName] = steps[i];
    const callFrames = await stepOverThenIn(name);
    checkTopFrameFunction(callFrames, expectedFunctionName);

    const resumedPromise = dp.Debugger.onceResumed();
    const nextPausedPromise =
        i + 1 < steps.length ? dp.Debugger.oncePaused() : null;
    await dp.Debugger.resume();
    await resumedPromise;
    if (nextPausedPromise) {
      await nextPausedPromise;
    }
  }

  const output = await session.evaluate(() => self._output);
  for (const line of output) {
    testRunner.log(line);
  }

  testRunner.completeTest();
});

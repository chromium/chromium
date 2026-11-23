// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that DOM Mutation Observers do not attempt to deliver mutation records while the debugger is paused.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  async function dumpDeliveryCount(callFrameId) {
    let response;
    if (callFrameId) {
      response = await dp.Debugger.evaluateOnCallFrame({
        callFrameId,
        expression: 'deliveryCount',
        objectGroup: 'console',
      });
    } else {
      response = await dp.Runtime.evaluate({
        expression: 'deliveryCount',
        objectGroup: 'console',
      });
    }
    if (response.error || response.result.exceptionDetails) {
      testRunner.fail(
          `Unexpected evaluation failure: ${JSON.stringify(response)}`);
      return;
    }
    testRunner.log(`deliveryCount = ${response.result.result.value}`);
  }

  await dp.Runtime.evaluate({
    expression: `
      window.testDiv = document.createElement('div');
      window.deliveryCount = 0;
      const observer = new MutationObserver(() => {
        window.deliveryCount++;
      });
      observer.observe(window.testDiv, {attributes: true});
    `,
    objectGroup: 'console',
  });
  testRunner.log('DIV and observer setup.');
  await dumpDeliveryCount();

  await dp.Runtime.evaluate({
    expression: `window.testDiv.setAttribute('foo', 'bar');`,
    objectGroup: 'console',
  });
  testRunner.log('setAttribute should have triggered delivery.');
  await dumpDeliveryCount();

  const pausedPromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate({
    expression: `
      function mutateAndPause() {
        window.testDiv.setAttribute('foo', 'baz');
        debugger;
      }
      setTimeout(mutateAndPause, 0);
    `,
    objectGroup: 'console',
  });
  testRunner.log('mutateAndPause invoked.');

  const {params: {callFrames}} = await pausedPromise;
  if (callFrames[0].functionName !== 'mutateAndPause') {
    testRunner.fail(
        `Expected pause in mutateAndPause, got ${callFrames[0].functionName}`);
    return;
  }
  testRunner.log('Script execution paused.');
  testRunner.log('Delivery should not have taken place.');
  await dumpDeliveryCount(callFrames[0].callFrameId);

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  testRunner.log('Script execution resumed.');
  testRunner.log('Second delivery should now have happened.');
  await dumpDeliveryCount();

  testRunner.completeTest();
});

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that custom element WebIDL lifecycle callbacks are suppressed while debugger is paused, while constructor runs and callbacks fire normally when unpaused.');
  await dp.Debugger.enable();

  await session.evaluate(`
    function output(message) {
      if (!self._output)
        self._output = [];
      self._output.push('[page] ' + message);
    }
  `);

  const setup = `
    class CustomElement extends HTMLElement {
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
      static get observedAttributes() { return ['x']; }
    }
    customElements.define('x-foo', CustomElement);
  `;

  const lifecycleCallbacks = `
    created = new CustomElement();
    created.setAttribute('x', '1');
    document.body.appendChild(created);
    created.remove();
    document.implementation.createHTMLDocument('').adoptNode(created);
  `;

  await session.evaluate(setup);
  testRunner.log('Custom element registered.');
  const beforePauseResult =
      await session.evaluate('new CustomElement() instanceof CustomElement');
  testRunner.log(
      `new CustomElement() instanceof CustomElement = ${beforePauseResult}`);

  await session.evaluate('self._output = []');
  await session.evaluate(lifecycleCallbacks);
  testRunner.log('Lifecycle callbacks while unpaused:');
  const unpausedOutputJson =
      await session.evaluate('JSON.stringify(self._output)');
  for (const line of JSON.parse(unpausedOutputJson))
    testRunner.log(line);
  await session.evaluate('self._output = []');

  const pausedPromise = dp.Debugger.oncePaused();
  const evalPromise = dp.Runtime.evaluate({expression: 'debugger;'});
  const {params: {callFrames}} = await pausedPromise;
  testRunner.log('Script execution paused.');
  const callFrameId = callFrames[0].callFrameId;

  const whilePausedResponse = await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'new CustomElement() instanceof CustomElement',
    returnByValue: true,
  });
  testRunner.log(`new CustomElement() instanceof CustomElement = ${
      whilePausedResponse.result.result.value}`);

  const lifecycleResponse = await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: lifecycleCallbacks,
  });
  if (lifecycleResponse.result.exceptionDetails) {
    testRunner.fail(`Unexpected exception: ${
        lifecycleResponse.result.exceptionDetails.text}`);
    return;
  }
  const pausedOutputResponse = await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'JSON.stringify(self._output)',
    returnByValue: true,
  });
  testRunner.log('Lifecycle callbacks while paused:');
  for (const line of JSON.parse(pausedOutputResponse.result.result.value))
    testRunner.log(line);
  await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'self._output = []',
  });

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  await evalPromise;
  testRunner.log('Script execution resumed.');

  await session.evaluate('0');
  const afterResumeJson =
      await session.evaluate('JSON.stringify(self._output)');
  const afterResume = JSON.parse(afterResumeJson);
  testRunner.log(`Output lines after resume: ${afterResume.length}`);
  for (const line of afterResume)
    testRunner.log(line);

  testRunner.completeTest();
})

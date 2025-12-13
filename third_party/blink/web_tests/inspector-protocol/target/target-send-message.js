(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests sendMessage with invalid message.');

  // TODO(johannes): We plan to retire the non-flattened mode, in which
  // case there's no need for this test. Or if we do want to keep it, it should
  // use child sessions. See also crbug.com/991325.
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
    flatten: false});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  const {params:{sessionId}} = await attachedPromise;

  const event0 = await dp.Target.onceReceivedMessageFromTarget();
  testRunner.log(event0.params.message);

  const testMessage = async message => {
    const event = dp.Target.onceReceivedMessageFromTarget();
    dp.Target.sendMessageToTarget({
      sessionId: sessionId,
      message,
    });
    testRunner.log((await event).params.message);
  };

  testRunner.log('JSON syntax error..');
  await testMessage("{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"1\", awaitPromise: true},\"id\":1}");

  testRunner.log('JSON with primitive value..');
  await testMessage("42");

  testRunner.log('JSON without method property..');
  await testMessage("{}");

  testRunner.log('JSON without id property..');
  await testMessage("{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"42\"}}");

  testRunner.log('Valid JSON..');
  await testMessage("{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"42\"},\"id\":1}");

  testRunner.completeTest();
})

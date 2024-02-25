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
  let {params:{sessionId}} = await attachedPromise;

  let p = dp.Target.onceReceivedMessageFromTarget();
  testRunner.log('JSON syntax error..');
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: "{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"1\", awaitPromise: true},\"id\":1}"
  });
  let {params:{message}} = await p;
  testRunner.log(message);

  testRunner.log('JSON with primitive value..');
  p = dp.Target.onceReceivedMessageFromTarget();
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: "42"
  });
  ({params:{message}} = await p);
  testRunner.log(message);

  testRunner.log('JSON without method property..');
  p = dp.Target.onceReceivedMessageFromTarget();
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: "{}"
  });
  ({params:{message}} = await p);
  testRunner.log(message);

  testRunner.log('JSON without id property..');
  p = dp.Target.onceReceivedMessageFromTarget();
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: "{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"42\"}}"
  });
  ({params:{message}} = await p);
  testRunner.log(message);

  testRunner.log('Valid JSON..');
  p = dp.Target.onceReceivedMessageFromTarget();
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: "{\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"42\"},\"id\":1}"
  });
  ({params:{message}} = await p);
  testRunner.log(message);
  testRunner.completeTest();
})

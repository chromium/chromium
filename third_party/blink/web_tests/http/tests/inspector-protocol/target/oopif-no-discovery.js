(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests oopif discovery.`);

  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('../resources/site_per_process_main.html')});
  await dp.Page.onceLoadEventFired();

  testRunner.log('Enabling auto-discovery...');
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});

  const attachedEvent = (await attachedPromise).params;
  testRunner.log('Got auto-attached.');
  const frameId = attachedEvent.targetInfo.targetId;

  testRunner.log('Navigating to in-process iframe...');
  const navigatePromise = dp.Page.navigate({frameId, url: testRunner.url('../resources/iframe.html')});
  const detachedPromise = dp.Target.onceDetachedFromTarget();
  await Promise.all([navigatePromise, detachedPromise]);

  const detachedEvent = (await detachedPromise).params;
  testRunner.log('Session id should match: ' + (attachedEvent.sessionId === detachedEvent.sessionId));
  testRunner.log('Target id should match: ' + (attachedEvent.targetInfo.targetId === detachedEvent.targetId));

  testRunner.log('Navigating back to out-of-process iframe...');
  // Wait for a little while before continuing, to ensure that the new
  // navigation won't get cancelled by the previous navigation's commit in the
  // browser process.
  await new Promise(resolve => setTimeout(resolve, 100));

  const attachedPromise2 = dp.Target.onceAttachedToTarget();
  dp.Page.navigate({frameId, url: 'http://devtools.oopif.test:8000/inspector-protocol/resources/iframe.html'});

  const attachedEvent2 = (await attachedPromise2).params;
  testRunner.log('Target ids should match: ' + (attachedEvent.targetInfo.targetId === attachedEvent2.targetInfo.targetId));

  const detachedPromise2 = dp.Target.onceDetachedFromTarget();
  await dp.Target.setAutoAttach({autoAttach: false, waitForDebuggerOnStart: false});
  const detachedEvent2 = (await detachedPromise2).params;
  testRunner.log('Session id should match: ' + (attachedEvent2.sessionId === detachedEvent2.sessionId));
  testRunner.log('Target id should match: ' + (attachedEvent2.targetInfo.targetId === detachedEvent2.targetId));

  testRunner.completeTest();
})

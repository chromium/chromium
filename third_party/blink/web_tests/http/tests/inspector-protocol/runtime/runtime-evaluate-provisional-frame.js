(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<html><body><iframe id='frame'></iframe></body></html>`,
      'Tests that Runtime.evaluate on a newly attached provisional OOPiF target does not crash and is queued until the navigation commits.');

  // Enable Page domain on the parent session to observe in-process navigations
  // if Site Isolation is disabled.
  await dp.Page.enable();

  // Pause new targets on start so we can attach to the speculative OOPiF target
  // while its LocalFrame is still provisional (before navigation commits).
  await dp.Target.setAutoAttach(
      {autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  const url =
      'http://oopif-a.devtools.test:8000/inspector-protocol/resources/empty.html';
  session.evaluate(`document.getElementById('frame').src = '${url}'`);

  // With Site Isolation enabled, the cross-origin navigation spawns an OOPiF
  // target and emits Target.attachedToTarget before commit. Without Site
  // Isolation, the iframe navigates in-process and emits Page.frameNavigated on
  // the parent session instead. Race both to fail fast instead of timing out.
  const event = await Promise.race([
    dp.Target.onceAttachedToTarget(),
    dp.Page.onceFrameNavigated(),
  ]);
  if (event.method !== 'Target.attachedToTarget') {
    testRunner.fail(
        'Frame navigated in-process instead of creating an OOPiF target (Site Isolation is required)');
    return;
  }
  const {params} = event;
  const childSession = session.createChild(params.sessionId);
  const childDp = childSession.protocol;

  // At this point the OOPiF target is paused before commit, so its frame is
  // provisional and the DevToolsSession is suspended until navigation commits.
  // Evaluating script must be queued until commit rather than crashing in
  // MainThreadDebugger::ensureDefaultContextInGroup().
  testRunner.log('Evaluating on provisional frame before navigation commits:');
  const evalPromise = childDp.Runtime.evaluate({expression: 'window.a = 1'});

  // Resume the target so the navigation commits and the frame swaps in.
  const pageEnablePromise = childDp.Page.enable();
  const domContentPromise = childDp.Page.onceDomContentEventFired();
  await childDp.Runtime.runIfWaitingForDebugger();
  await pageEnablePromise;
  testRunner.log(await evalPromise);
  await domContentPromise;

  // Once committed, Runtime.evaluate should succeed on the active document.
  testRunner.log('Evaluating after navigation commits:');
  testRunner.log(await childDp.Runtime.evaluate({expression: 'window.a'}));

  testRunner.completeTest();
})

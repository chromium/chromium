(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests basic functionality of Tatget.autoAttachRelated.');
  const bp = testRunner.browserP();

  const createChildFrame = function(url) {
    return new Promise(resolve => {
      const frame = document.createElement(`iframe`);
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  }

  async function expectTargetsDetached(count) {
    for (let i = 0; i < count; ++i)
      await bp.Target.onceDetachedFromTarget();
    testRunner.log(`Detached from ${count} targets`);
  }

  const page1 = await testRunner.createPage();
  const page2 = await testRunner.createPage();
  const page1_session = await page1.createSession();
  const page2_session = await page2.createSession();

  bp.Target.onAttachedToTarget(event => {
    const params = event.params;
    testRunner.log(`Attached to ${params.targetInfo.type} target: ${params.targetInfo.url} (waiting: ${params.waitingForDebugger})`);
  });

  // page1 will auto-attach, but page2 will not.
  await bp.Target.autoAttachRelated({targetId: page1.targetId(), waitForDebuggerOnStart: false});

  page1_session.evaluate(`(${createChildFrame})('http://devtools.oopif.test:8080/inspector-protocol/resources/iframe.html')`);
  const frame1_attached = (await bp.Target.onceAttachedToTarget()).params;
  const frame1SessionId = frame1_attached.sessionId;
  const frame1TargetId = frame1_attached.targetInfo.targetId;
  const frame1_session = new TestRunner.Session(testRunner, frame1SessionId);

  await page2_session.evaluateAsync(`(${createChildFrame})('https://devtools.oopif.test:8443/inspector-protocol/resources/iframe.html')`);

  // This one should auto-attach and wait.
  await bp.Target.autoAttachRelated({targetId: frame1TargetId, waitForDebuggerOnStart: true});
  frame1_session.evaluate(`(${createChildFrame})('http://inner-frame.test:8080/inspector-protocol/resources/iframe.html')`);

  const frame11SessionId = (await bp.Target.onceAttachedToTarget()).params.sessionId;
  const frame11_dp = (new TestRunner.Session(testRunner, frame11SessionId)).protocol;
  await frame11_dp.Runtime.runIfWaitingForDebugger();

  // Change waitForDebuggerOnStart for the tartget we already observe...
  await bp.Target.autoAttachRelated({targetId: page1.targetId(), waitForDebuggerOnStart: true});
  // and assure it has the effect.
  page1_session.evaluate(`(${createChildFrame})('http://devtools.oopif.test:8080/inspector-protocol/resources/iframe.html?frame3')`);
  const frame3SessionId = (await bp.Target.onceAttachedToTarget()).params.sessionId;
  const frame3_dp = (new TestRunner.Session(testRunner, frame3SessionId)).protocol;
  frame3_dp.Runtime.runIfWaitingForDebugger();

  page1_session.evaluate(`document.body.textContent='';`);
  await expectTargetsDetached(3);

  testRunner.log(`Disalbing auto-attach`);
  // Disable auto-attach altogether.
  bp.Target.setAutoAttach({autoAttach: false, waitForDebuggerOnStart: false, flatten: true});
  await page1_session.evaluate(`(${createChildFrame})('http://devtools.oopif.test:8080/inspector-protocol/resources/iframe.html')`);
  testRunner.log(`Re-enabling auto-attach for page1`);
  bp.Target.autoAttachRelated({targetId: page1.targetId(), waitForDebuggerOnStart: false});
  await bp.Target.onceAttachedToTarget();
  await bp.Target.onceAttachedToTarget();
  // Now disable auto-attach again and assure the target is detached.
  bp.Target.setAutoAttach({autoAttach: false, waitForDebuggerOnStart: false, flatten: true});
  await expectTargetsDetached(1);

  testRunner.log('DONE');
  testRunner.completeTest();
});

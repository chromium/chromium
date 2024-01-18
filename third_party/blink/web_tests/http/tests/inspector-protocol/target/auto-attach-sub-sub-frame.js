(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that setAutoAttach properly attaches OOPIF subframes 2 levels deep.`);

  const createChildFrame = function(url) {
    return new Promise(resolve => {
      const frame = document.createElement(`iframe`);
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  }
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const f1_attached = dp.Target.onceAttachedToTarget();
  await session.evaluateAsync(`
    (${createChildFrame.toString()})('http://devtools.oopif-a.test:8080/inspector-protocol/resources/iframe.html')
  `);

  const f1_session = session.createChild((await f1_attached).params.sessionId);
  const f1_dp = f1_session.protocol;

  // Log attach / detach events from subframe target, assure we're not missing any.
  f1_dp.Target.onAttachedToTarget(event => testRunner.log(`Attached to target ${event.params.targetInfo.url}`));
  f1_dp.Target.onDetachedFromTarget(() => testRunner.log('Detached from target'));

  await f1_dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  await f1_session.evaluateAsync(`
    (${createChildFrame.toString()})('http://devtools.oopif-b.test:8080/inspector-protocol/resources/iframe.html')
  `);

  await f1_dp.Target.setAutoAttach({autoAttach: false, waitForDebuggerOnStart: false, flatten: true});

  await f1_dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  testRunner.completeTest();
})

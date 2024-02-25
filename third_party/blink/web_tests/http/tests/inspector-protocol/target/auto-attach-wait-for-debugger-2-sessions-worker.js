(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that worker is only resumed when all sessions issue runIfWaitingForDebugger.`);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const dp2 = (await page.createSession()).protocol;

  await dp2.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});


  session.evaluate(`
    self.worker = new Worker('/inspector-protocol/network/resources/worker.js');
  `);

  const attached = await Promise.all([
    dp.Target.onceAttachedToTarget(),
    dp2.Target.onceAttachedToTarget()
  ]);

  testRunner.log(attached.map(event => event.params.targetInfo), 'Attached 2 sessions:');

  const worker_session1 = (session.createChild(attached[0].params.sessionId));
  const worker_session2 = (session.createChild(attached[1].params.sessionId));
  const [wp1, wp2] = [worker_session1.protocol, worker_session2.protocol];

  wp1.Runtime.enable();
  wp2.Runtime.enable();

  wp1.Runtime.onConsoleAPICalled(event => testRunner.log('Session 1: ' + event.params.args[0].value));
  wp2.Runtime.onConsoleAPICalled(event => testRunner.log('Session 2: ' + event.params.args[0].value));

  await wp1.Runtime.runIfWaitingForDebugger();
  testRunner.log(`Session 1 resumed!`)

  await wp1.Runtime.runIfWaitingForDebugger();
  testRunner.log(`Session 2 resumed!`);

  await worker_session1.evaluate('');

  testRunner.completeTest();
})


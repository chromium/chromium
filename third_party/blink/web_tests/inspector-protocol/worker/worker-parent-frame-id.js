(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('TargetInfo.parentFrameId is set correctly for workers.');

  await session.evaluateAsync(`
    window.iframe1 = document.createElement('iframe');
    iframe1.src = 'about:blank';
    document.body.appendChild(iframe1);

    window.iframe2 = iframe1.contentWindow.document.createElement('iframe');
    iframe2.src = 'about:blank';
    iframe1.contentWindow.document.body.appendChild(iframe2);
  `);

  const frameTree = (await dp.Page.getFrameTree()).result.frameTree;

  await dp.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  testRunner.log('worker in main frame');
  session.evaluateAsync(`
    {
      const win = window;
      win.worker = new win.Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    }
  `);
  const event0 = await dp.Target.onceAttachedToTarget();
  testRunner.log('parentFrameId matches: ' + (event0.params.targetInfo.parentFrameId === frameTree.frame.id));

  testRunner.log('worker in child frame');
  session.evaluateAsync(`
    {
      const win = iframe1.contentWindow;
      win.worker = new win.Worker('${testRunner.url('../resources/worker-console-worker.js')}');
  }
  `);
  const event1 = await dp.Target.onceAttachedToTarget();
  testRunner.log('parentFrameId matches: ' + (event1.params.targetInfo.parentFrameId === frameTree.childFrames[0].frame.id));

  testRunner.log('worker in grand-child frame');
  session.evaluateAsync(`
    {
      const win = iframe2.contentWindow;
      win.worker = new win.Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    }
  `);
  const event2 = await dp.Target.onceAttachedToTarget();
  testRunner.log('parentFrameId matches: ' + (event2.params.targetInfo.parentFrameId === frameTree.childFrames[0].childFrames[0].frame.id));

  testRunner.completeTest();
})

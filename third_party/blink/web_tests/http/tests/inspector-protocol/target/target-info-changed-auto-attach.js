(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that Target.targetInfoChanged is dispatched for auto-attached targets.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  session.evaluate(`
    const iframe = document.createElement('iframe');
    iframe.src = 'http://devtools.oopif-a.test:8000/inspector-protocol/resources/inspector-protocol-page.html';
    document.body.appendChild(iframe);
  `);
  const target = (await dp.Target.onceAttachedToTarget()).params;
  testRunner.log(target.targetInfo);
  const info = (await dp.Target.onceTargetInfoChanged()).params;
  testRunner.log(info);

  session.evaluate(`
    iframe.src = 'http://devtools.oopif-a.test:8000/inspector-protocol/resources/inspector-protocol-page.html?foo'
  `);

  const info2 = (await dp.Target.onceTargetInfoChanged()).params;
  testRunner.log(info2);

  testRunner.completeTest();
})

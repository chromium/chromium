(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Target.setAutoAttach(windowOpen=true) attaches to window.open targets.`);

  await dp.Target.setDiscoverTargets({discover: true});

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true, windowOpen: true});

  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened the window');
  await dp.Target.onceAttachedToTarget();
  testRunner.log('Attached to window');

  session.evaluate(`
    window.myWindow.location.assign('../resources/inspector-protocol-page.html?foo'); undefined;
  `);
  testRunner.log('Navigated the window');
  await dp.Target.onceTargetInfoChanged();
  testRunner.log('Target info changed');

  session.evaluate(`
    window.myWindow.close(); undefined;
  `);
  testRunner.log('Closed the window');
  await dp.Target.onceDetachedFromTarget();
  testRunner.log('Detached from window');

  testRunner.completeTest();
})

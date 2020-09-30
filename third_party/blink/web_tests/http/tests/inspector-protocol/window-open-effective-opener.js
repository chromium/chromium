(async function(testRunner) {
    var {page, session, dp} = await testRunner.startBlank('Tests for correct opener in Page.TargetCreated and Page.TargetInfoChanged protocol method.');
    await dp.Target.setDiscoverTargets({discover: true});
    await dp.Page.enable();

    session.navigate('./resources/empty.html');
    var response = await dp.Target.onceTargetInfoChanged();
    const targetId = response.params.targetInfo.targetId;

    function compareTargetIds(expectedId, actualId, keyName) {
      if (expectedId === actualId)
        testRunner.log(`PASS: Correct ${keyName}`);
      else
        testRunner.log(`FAIL: Incorrect ${keyName}`);
    }

    testRunner.log(`\nOpening without new browsing context`);
    session.evaluate(`window.open('./resources/test-page.html', '_blank'); undefined`);
    response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');
    response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');

    testRunner.log(`\nOpening with new browsing context`);
    session.evaluate(`window.open('./resources/test-page.html', '_blank', 'noopener'); undefined`);
    response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');
    response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');

    testRunner.log(`\nOpening with COOP header`);
    await dp.Page.navigate({ url: testRunner.url('https://127.0.0.1:8443/inspector-protocol/resources/coop.php')});
    session.evaluate(`window.open('./resources/test-page.html', '_blank'); undefined`);
    response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');
    response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);
    compareTargetIds(targetId, response.params.targetInfo.openerFrameId, 'openerFrameId');
    compareTargetIds(targetId, response.params.targetInfo.openerId, 'openerId');

    testRunner.completeTest();
  })
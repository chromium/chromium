(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session: firstSession, dp } = await testRunner.startURL(
    'resources/css-poll-style-updates.html',
    'Test CSS.trackComputedStyleUpdates and CSS.takeComputedStyleUpdates methods');

  async function setupAndGetNodeIds(protocol) {
    await protocol.DOM.enable();
    await protocol.CSS.enable();

    const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
    const cssHelper = new CSSHelper(testRunner, protocol);

    const documentNodeId = await cssHelper.requestDocumentNodeId();
    const nodeIds = await cssHelper.requestAllNodeIds(documentNodeId, '.item');
    return { protocol, nodeIds };
  }

  const { protocol: firstDP, nodeIds: firstSessionNodeIds } =
    await setupAndGetNodeIds(dp);
  const sencondSession = await page.createSession();
  const { protocol: secondDP, nodeIds: secondSessionNodeIds } =
    await setupAndGetNodeIds(sencondSession.protocol);

  await firstDP.CSS.trackComputedStyleUpdates({
    'propertiesToTrack': [
      {
        name: 'display',
        value: 'grid',
      },
      {
        name: 'width',
        value: '100%',
      }
    ],
  });

  firstSession.evaluate(
    () => document.querySelector('.container').classList.add('change-1'));
  // Tracked style updates should send polling result back in a batch,
  // and Untracked style updates should not be sent back.
  const firstRoundResponse = await firstDP.CSS.takeComputedStyleUpdates();
  const firstRoundNodeIds = firstRoundResponse.result.nodeIds;
  testRunner.log(`First round of updated nodes should contain the first item: ${
    firstRoundNodeIds.includes(firstSessionNodeIds[0])}`);
  testRunner.log(
    `First round of updated nodes should not contain the second item: ${
    !firstRoundNodeIds.includes(firstSessionNodeIds[1])}`);
  testRunner.log(
    `First round of updated nodes should contain the third item: ${
    firstRoundNodeIds.includes(firstSessionNodeIds[2])}`);
  testRunner.log(
    `First round of updated nodes should not contain the fourth item: ${
    !firstRoundNodeIds.includes(firstSessionNodeIds[3])}`);

  // Tests multiple sessions can track styles successfully and
  // multiple values of the same CSS property should all be tracked correctly.
  await firstDP.CSS.trackComputedStyleUpdates({
    'propertiesToTrack': [
      {
        name: 'position',
        value: 'relative',
      },
      {
        name: 'position',
        value: 'absolute',
      }
    ],
  });

  await secondDP.CSS.trackComputedStyleUpdates({
    'propertiesToTrack': [
      {
        name: 'display',
        value: 'flex',
      },
      {
        name: 'display',
        value: 'grid',
      }
    ],
  });

  firstSession.evaluate(
    () => document.querySelector('.container').classList.add('change-2'));
  sencondSession.evaluate(
    () => document.querySelector('.container').classList.add('change-3'));
  const [firstSessionResponse, secondSessionResponse] = await Promise.all([
    firstDP.CSS.takeComputedStyleUpdates(),
    secondDP.CSS.takeComputedStyleUpdates(),
  ]);
  const firstSessionRespondedIds = firstSessionResponse.result.nodeIds;
  testRunner.log(
    `Updated nodes from the first session should contain the first item: ${
    firstSessionRespondedIds.includes(firstSessionNodeIds[0])}`);
  testRunner.log(
    `Updated nodes from the first session should contain the second item: ${
    firstSessionRespondedIds.includes(firstSessionNodeIds[1])}`);
  testRunner.log(
    `Updated nodes from the first session should not contain the third item: ${
    !firstSessionRespondedIds.includes(firstSessionNodeIds[2])}`);
  testRunner.log(
    `Updated nodes from the first session should not contain the fourth item: ${
    !firstSessionRespondedIds.includes(firstSessionNodeIds[3])}`);

  const secondSessionRespondedIds = secondSessionResponse.result.nodeIds;
  testRunner.log(
    `Updated nodes from the second session should not contain the first item: ${
    !secondSessionRespondedIds.includes(secondSessionNodeIds[0])}`);
  testRunner.log(
    `Updated nodes from the second session should contain the second item: ${
    secondSessionRespondedIds.includes(secondSessionNodeIds[1])}`);
  testRunner.log(
    `Updated nodes from the second session should contain the third item: ${
    secondSessionRespondedIds.includes(secondSessionNodeIds[2])}`);
  testRunner.log(
    `Updated nodes from the second session should not contain the fourth item: ${
    !secondSessionRespondedIds.includes(secondSessionNodeIds[3])}`);

  await firstDP.CSS.trackComputedStyleUpdates({
    'propertiesToTrack': [],
  });

  errorResponse = await firstDP.CSS.takeComputedStyleUpdates();
  testRunner.log(
    'Sending a request while no style is being tracked should fail with an error message:');
  testRunner.log(errorResponse.error);

  testRunner.completeTest();
});

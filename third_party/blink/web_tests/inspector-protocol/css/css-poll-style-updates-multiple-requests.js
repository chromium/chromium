(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startURL(
    'resources/css-poll-style-updates.html',
    'Test multiple CSS.takeComputedStyleUpdates commands at the same time.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeIds = await cssHelper.requestAllNodeIds(documentNodeId, '.item');

  await dp.CSS.trackComputedStyleUpdates({
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

  // Test multiple requests at the same time
  const multipleRequestsPromise = Promise.all([
    dp.CSS.takeComputedStyleUpdates(),
    dp.CSS.takeComputedStyleUpdates(),
  ]);

  session.evaluate(
    () =>
      document.querySelector('.container').classList.add('change-2'));
  // Only one request can be active at a time; other requests should fail.
  const multipleRequestsResponse = await multipleRequestsPromise;
  let errorResponse =
    multipleRequestsResponse.find(result => Boolean(result.error));
  testRunner.log(
    'Sending a request before the previous one is resolved should fail with an error message:');
  testRunner.log(errorResponse && errorResponse.error);

  const pollBeforeDisablePromise = dp.CSS.takeComputedStyleUpdates();
  await dp.CSS.trackComputedStyleUpdates({
    'propertiesToTrack': [],
  });
  testRunner.log((await pollBeforeDisablePromise).result.nodeIds);

  // Re-enable tracking and poll to see if the previous callback was correctly nullified
  await dp.CSS.trackComputedStyleUpdates({
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

  session.evaluate(
    () =>
      document.querySelector('.container').classList.remove('change-2'));

  const pollAfterReEnableResult = await dp.CSS.takeComputedStyleUpdates();
  const respondedIds = pollAfterReEnableResult.result.nodeIds;
  testRunner.log(
    `Updated nodes should contain the first item: ${
      respondedIds.includes(nodeIds[0])}`);
  testRunner.log(
    `Updated nodes should contain the second item: ${
      respondedIds.includes(nodeIds[1])}`);
  testRunner.log(
    `Updated nodes should not contain the third item: ${
    !respondedIds.includes(nodeIds[2])}`);
  testRunner.log(
    `Updated nodes should not contain the fourth item: ${
    !respondedIds.includes(nodeIds[3])}`);
  testRunner.completeTest();
});

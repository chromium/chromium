(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://example.test:8443/inspector-protocol/resources/permissions-policy-page.php',
      'Verifies that we can successfully retrieve permissions policy state in frame tree');

  await dp.Page.enable();
  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const result = (await dp.Page.getPermissionsPolicyState({frameId})).result;
  const states = result.states
    .sort((a, b) => a.feature.localeCompare(b.feature))
    .filter(state => !state.allowed);
  testRunner.log(states);
  testRunner.log((await dp.Page.getPermissionsPolicyState({frameId: 'bad-id'})));
  testRunner.completeTest();
});

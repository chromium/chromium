(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Page.reload applies scriptToEvaluateOnLoad to the reload of the main frame only.`);

  const FetchHelper =
      await testRunner.loadScript('../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  helper.setEnableLogging(false);
  await helper.enable();

  await dp.Page.enable();

  const frameBody = '<html><body>frame</body></html>';

  function pageBody(framePath) {
    return `<html><body><iframe src="${framePath}"></iframe></body></html>`;
  }

  async function navigateToPageWithFrame(pagePath, framePath) {
    helper.onceRequest(new RegExp(`a\\.test${pagePath}$`)).fulfill({
      responseCode: 200,
      body: btoa(pageBody(framePath)),
    });
    helper.onceRequest(new RegExp(`a\\.test${framePath}$`)).fulfill({
      responseCode: 200,
      body: btoa(frameBody),
    });
    const loaded = dp.Page.onceLoadEventFired();
    await dp.Page.navigate({url: `http://a.test${pagePath}`});
    await loaded;
  }

  // Starts a subframe navigation and waits for its request to arrive,
  // without responding to it yet.
  async function startSubframeNavigation(expression, framePath) {
    const subframeRequest =
        helper.onceRequest(new RegExp(`a\\.test${framePath}$`));
    const matched = subframeRequest.matched();
    await session.evaluate(expression);
    return matched;
  }

  // Issues Page.reload with a script and waits for the main frame re-fetch
  // to arrive, without responding to it yet.
  async function startReloadWithScript(pagePath) {
    const reloadRequest =
        helper.onceRequest(new RegExp(`a\\.test${pagePath}$`));
    const reloadPromise = dp.Page.reload(
        {scriptToEvaluateOnLoad: `window.__injected = "${pagePath}";`});
    const request = await reloadRequest.matched();
    return {request, reloadPromise};
  }

  async function checkReloadResponse(reloadPromise) {
    const response = await reloadPromise;
    if (response.error) {
      testRunner.fail(`Page.reload failed: ${response.error.message}`);
    }
  }

  async function commitSubframeNavigation(request) {
    const subframeNavigated =
        dp.Page.onceFrameNavigated(event => !!event.params.frame.parentId);
    testRunner.browserP().Fetch.fulfillRequest({
      requestId: request.requestId,
      responseCode: 200,
      body: btoa(frameBody),
    });
    await subframeNavigated;
  }

  function fulfillHeldReload(request, framePath) {
    helper.onceRequest(new RegExp(`a\\.test${framePath}$`)).fulfill({
      responseCode: 200,
      body: btoa(frameBody),
    });
    testRunner.browserP().Fetch.fulfillRequest({
      requestId: request.requestId,
      responseCode: 200,
      body: btoa(pageBody(framePath)),
    });
  }

  // 1. A subframe navigation committing while the reload is in flight must
  // not cancel the script.
  testRunner.log('\n--- Subframe navigates while reload is in flight ---');
  await navigateToPageWithFrame('/pageA', '/frameA');
  const subframeA = await startSubframeNavigation(
      'frames[0].location.href = "/frameA2";', '/frameA2');
  const reloadA = await startReloadWithScript('/pageA');
  await commitSubframeNavigation(subframeA);
  const reloadALoaded = dp.Page.onceLoadEventFired();
  fulfillHeldReload(reloadA.request, '/frameA');
  await reloadALoaded;
  await checkReloadResponse(reloadA.reloadPromise);
  testRunner.log(
      `window.__injected: ${await session.evaluate('window.__injected')}`);

  // 2. Regression guard: a subframe reload committing while the main-frame
  // reload is in flight must leave the script intact for the main-frame reload
  // when it commits.
  testRunner.log('\n--- Subframe reloads while reload is in flight ---');
  await navigateToPageWithFrame('/pageB', '/frameB');
  const subframeB =
      await startSubframeNavigation('frames[0].location.reload();', '/frameB');
  const reloadB = await startReloadWithScript('/pageB');
  await commitSubframeNavigation(subframeB);
  const reloadBLoaded = dp.Page.onceLoadEventFired();
  fulfillHeldReload(reloadB.request, '/frameB');
  await reloadBLoaded;
  await checkReloadResponse(reloadB.reloadPromise);
  testRunner.log(
      `window.__injected: ${await session.evaluate('window.__injected')}`);

  // 3. If the reload never commits, the script must not apply to a later
  // navigation.
  testRunner.log('\n--- Reload is canceled after a subframe reload ---');
  await navigateToPageWithFrame('/pageC', '/frameC');
  const subframeC =
      await startSubframeNavigation('frames[0].location.reload();', '/frameC');
  const reloadC = await startReloadWithScript('/pageC');
  await commitSubframeNavigation(subframeC);
  // A 204 response cancels the pending reload without committing it.
  await testRunner.browserP().Fetch.fulfillRequest(
      {requestId: reloadC.request.requestId, responseCode: 204});
  await checkReloadResponse(reloadC.reloadPromise);
  helper.onceRequest(/b\.test\/next$/).fulfill({
    responseCode: 200,
    body: btoa('<html><body>next</body></html>'),
  });
  const nextLoaded = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({url: 'http://b.test/next'});
  await nextLoaded;
  testRunner.log(
      `window.__injected: ${await session.evaluate('window.__injected')}`);

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // 1. Start with a valid page and load the service worker helper.
  const {page, session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      'Tests that Network.setUserAgentOverride correctly affects navigator.userAgentData in a service worker context.');
  const swHelper =
      (await testRunner.loadScript('resources/service-worker-helper.js'))(
          dp, session);

  const overriddenBrands = [
    {'brand': 'TestBrand', 'version': '123'},
    {'brand': ' Not A;Brand', 'version': '99'}
  ];
  const userAgentMetadata = {
    brands: overriddenBrands,
    fullVersion: '123.0.0.0',
    platform: 'TestPlatform',
    platformVersion: '1.0',
    architecture: 'x64',
    model: 'TestModel',
    mobile: false
  };

  testRunner.log('Test started.');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  testRunner.log('Target auto-attach enabled.');

  const serviceWorkerURL =
      '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'service_worker');
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);
  testRunner.log('Service worker installation initiated.');

  const attachedToTarget = await attachedPromise;
  testRunner.log('Attached to service worker target.');

  const swdp = session.createChild(attachedToTarget.params.sessionId).protocol;

  await swdp.Network.enable();
  await swdp.Network.setUserAgentOverride({
    userAgent: 'Overridden UA String',
    userAgentMetadata: userAgentMetadata
  });
  testRunner.log(
      'Network.setUserAgentOverride sent to the service worker target.');

  const {result: {result: {value: brands}}} = await swdp.Runtime.evaluate({
    expression: 'navigator.userAgentData.brands',
    awaitPromise: false,
    returnByValue: true,
  });

  testRunner.log(`Service worker navigator.userAgentData.brands:`);
  testRunner.log(brands);

  testRunner.completeTest();
})

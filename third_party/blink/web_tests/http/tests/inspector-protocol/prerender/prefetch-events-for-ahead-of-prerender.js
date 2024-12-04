(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender navigations receives the status updates of prefetch ahead of prerender`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp = session.protocol;
  await dp.Preload.enable();
  await dp.Network.enable();

  // @param {Session} session
  // @param {[string]} methodNames
  // @param {(Event, [Event]) -> bool} quitPred; The first argument is current
  // event. The second argument is the collected events, including the current
  // one.
  // @return {Promise<[Event]>}
  //
  // Listen events with `methodNames` until `quitPred` returns true.
  function sessionListenMethods(session, methodNames, quitPred) {
    const {promise, resolve} = Promise.withResolvers();

    let collected = [];

    const eventHandler = event => {
      collected.push(event);

      if (quitPred(event, collected)) {
        for (methodName of methodNames) {
          session._removeEventHandler(methodName, eventHandler);
        }
        resolve(collected);
      }
    };

    for (methodName of methodNames) {
      session._addEventHandler(methodName, eventHandler);
    }

    return promise;
  }
  const networkEventsPromise = sessionListenMethods(
      session,
      [
        'Network.requestWillBeSent',
        'Network.requestWillBeSentExtraInfo',
        'Network.responseReceived',
        'Network.responseReceivedExtraInfo',
        'Network.loadingFinished',
        'Network.loadingFailed',
      ],
      (_event, collected) =>
          collected.filter(event => event.method === 'Network.loadingFinished')
              .length == 2);

  session.navigate('resources/simple-prerender.html');

  // Prefetch, TriggeringOutcome = Running
  testRunner.log(await dp.Preload.oncePrefetchStatusUpdated());

  // Prerender, TriggeringOutcome = Pending
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  // Prerender, TriggeringOutcome = Running
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  // Prefetch, PrefetchStatus = PrefetchResponseUsed
  testRunner.log(await dp.Preload.oncePrefetchStatusUpdated());

  // Prerender, TriggeringOutcome = Ready
  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  // To avoid flakiness, we just see 'Network.requestWillBeSent' events.
  const networkEvents =
      (await networkEventsPromise)
          .filter(event => event.method === 'Network.requestWillBeSent');
  testRunner.log(networkEvents, '', [
    ...TestRunner.stabilizeNames,
    'Content-Length',
    'Date',
    'ETag',
    'Last-Modified',
    'User-Agent',
    'connectionId',
    'encodedDataLength',
    'headersText',
    'receiveHeadersEnd',
    'receiveHeadersStart',
    'requestTime',
    'responseTime',
    'sendEnd',
    'sendStart',
    'wallTime',
  ]);

  testRunner.completeTest();
});

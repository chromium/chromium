(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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

  async function
  prefetchStatusUpdatedShouldBeEmittedForPrefetchAheadOfPrerender() {
    const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
        `Preload.prefetchStatusUpadted should be emitted for prefetch ahead of prerender`);

    const childTargetManager =
        new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
    await childTargetManager.startAutoAttach();
    const session = childTargetManager.findAttachedSessionPrimaryMainFrame();
    const dp = session.protocol;
    await dp.Preload.enable();
    await dp.Network.enable();

    const preloadEventsPromise = sessionListenMethods(
        session,
        [
          'Preload.prefetchStatusUpdated',
          'Preload.prerenderStatusUpdated',
        ],
        (event) => event.method === 'Preload.prerenderStatusUpdated' &&
            event.params.status === 'Ready');
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
            collected
                .filter(event => event.method === 'Network.loadingFinished')
                .length == 2);

    await session.navigate('resources/simple-prerender.html');

    const preloadEvents = await preloadEventsPromise;
    const pipelineIds =
        [...new Set(preloadEvents.map(event => event.params.pipelineId))];
    for (event of preloadEvents) {
      event.params.pipelineIdIndex =
          pipelineIds.indexOf(event.params.pipelineId);
    }
    testRunner.log(preloadEvents);

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
  }

  async function pipelineIdShouldDifferIfMigrated() {
    const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
        `pipelineId should differ if a prefetch is triggered and then a prerender is triggered`);

    const childTargetManager =
        new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
    await childTargetManager.startAutoAttach();
    const session = childTargetManager.findAttachedSessionPrimaryMainFrame();
    const dp = session.protocol;
    await dp.Preload.enable();

    const preloadEventsPromise = sessionListenMethods(
        session,
        [
          'Preload.prefetchStatusUpdated',
          'Preload.prerenderStatusUpdated',
        ],
        (event) => event.method === 'Preload.prerenderStatusUpdated' &&
            event.params.status === 'Ready');

    await session.navigate('resources/empty.html');

    session.evaluate(() => {
      const script = document.createElement('script');
      script.type = 'speculationrules';
      script.text = `
      {
        "prefetch":[
          {
            "source": "list",
            "urls": ["/inspector-protocol/prerender/resources/empty.html"]
          }
        ]
      }`;
      document.body.appendChild(script);
    });

    await dp.Preload.onPrefetchStatusUpdated(
        event => event.params.status === 'Ready');

    session.evaluate(() => {
      const script = document.createElement('script');
      script.type = 'speculationrules';
      script.text = `
      {
        "prerender":[
          {
            "source": "list",
            "urls": ["/inspector-protocol/prerender/resources/empty.html"]
          }
        ]
      }`;
      document.body.appendChild(script);
    });

    const preloadEvents = await preloadEventsPromise;
    const pipelineIds =
        [...new Set(preloadEvents.map(event => event.params.pipelineId))];
    for (event of preloadEvents) {
      event.params.pipelineIdIndex =
          pipelineIds.indexOf(event.params.pipelineId);
    }
    testRunner.log(preloadEvents);
  }

  testRunner.runTestSuite([
    prefetchStatusUpdatedShouldBeEmittedForPrefetchAheadOfPrerender,
    pipelineIdShouldDifferIfMigrated,
  ]);
});

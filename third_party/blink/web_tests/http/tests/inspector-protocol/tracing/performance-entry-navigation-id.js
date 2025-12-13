(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests navigationId in performance entries.');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  // Simulates a user clicking a particular element, identified by its id.
  async function userClick(targetElementId) {
    const clickTargetCenter = await session.evaluate(function(targetElementId) {
      const rect =
          document.getElementById('click-target').getBoundingClientRect();
      return {x: (rect.left + rect.width) / 2, y: (rect.top + rect.height) / 2};
    }, targetElementId);
    await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      clickCount: 1,
      x: clickTargetCenter.x,
      y: clickTargetCenter.y
    });
    await dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      clickCount: 1,
      x: clickTargetCenter.x,
      y: clickTargetCenter.y
    });
  }

  async function hardNavigation() {
    return await session.evaluate(function() {
      const entry = performance.getEntriesByType('navigation')[0];
      return {navigationId: entry.navigationId, startTime: entry.startTime};
    });
  }

  async function firstEntryAfter(timeStamp, entryType) {
    return session.evaluateAsync(
      function (timeStamp, entryType) {
          return new Promise(resolve => {
            new PerformanceObserver((list, observer) => {
              const e =
                  list.getEntries().filter(e => e.startTime >= timeStamp)[0];
              if (e !== undefined) {
                resolve({navigationId: e.navigationId, startTime: e.startTime});
                observer.disconnect();
              }
            }).observe({
              type: entryType,
              buffered: true
            });
          });
        },
      timeStamp, entryType);
  }

  // Start tracing and observe the devtools.timeline category.
  await tracingHelper.startTracing('devtools.timeline');

  // Navigate to a basic page (hard navigation) which generates a text LCP.
  const textLcpUrl =
      'http://127.0.0.1:8000/inspector-protocol/resources/text-lcp.html';
  await dp.Page.navigate({url: textLcpUrl});

  const firstHardNav = await hardNavigation();

  const perfEntries = [];
  perfEntries.push({
    navigationId: firstHardNav.navigationId,
    name: 'hard-navigation (entry for text-lcp.html)'
  });

  const firstHardLCP =
      await firstEntryAfter(firstHardNav.startTime, 'largest-contentful-paint');
  perfEntries.push(
      {navigationId: firstHardLCP.navigationId, name: 'hard-lcp (entry)'});

  // Navigate to an empty page (hard navigation).
  const emptyPageUrl =
      'http://127.0.0.1:8000/inspector-protocol/resources/empty.html';
  await dp.Page.navigate({url: emptyPageUrl});

  const secondHardNav = await hardNavigation();
  perfEntries.push({
    navigationId: secondHardNav.navigationId,
    name: 'hard-navigation (entry for empty.html)'
  });

  const softNavUrl =
      'http://127.0.0.1:8000/inspector-protocol/resources/soft-navigations.html';
  await dp.Page.navigate({url: softNavUrl});

  const thirdHardNav = await hardNavigation();
  perfEntries.push({
    navigationId: thirdHardNav.navigationId,
    name: 'hard-navigation (entry for soft-navigations.html)'
  });

  const secondHardLCP =
      await firstEntryAfter(thirdHardNav.startTime, 'largest-contentful-paint');
  perfEntries.push({
    navigationId: secondHardLCP.navigationId,
    name: 'hard-lcp (entry) after soft-navigations.html'
  });

  await userClick('click-target');

  const firstSoftNav =
      await firstEntryAfter(thirdHardNav.startTime + 1, 'soft-navigation');
  perfEntries.push({
    navigationId: firstSoftNav.navigationId,
    name: 'soft-navigation (entry after 1st user click)'
  });

  const softLCP = await firstEntryAfter(
      firstSoftNav.startTime, 'interaction-contentful-paint', true);
  perfEntries.push(
      {navigationId: softLCP.navigationId, name: 'soft-lcp (entry)'});

  await userClick('click-target');

  const secondSoftNav =
      await firstEntryAfter(firstSoftNav.startTime + 1, 'soft-navigation');
  perfEntries.push({
    navigationId: secondSoftNav.navigationId,
    name: 'soft-navigation (entry after 2nd user click)'
  });

  const secondSoftLCP = await firstEntryAfter(
      secondSoftNav.startTime, 'interaction-contentful-paint', true);
  perfEntries.push(
      {navigationId: secondSoftLCP.navigationId, name: 'soft-lcp (entry)'});

  await userClick('click-target');

  const thirdSoftNav =
      await firstEntryAfter(secondSoftNav.startTime + 1, 'soft-navigation');
  perfEntries.push({
    navigationId: thirdSoftNav.navigationId,
    name: 'soft-navigation (entry after 3rd user click)'
  });

  const thirdSoftLCP = await firstEntryAfter(
      thirdSoftNav.startTime, 'interaction-contentful-paint', true);
  perfEntries.push(
      {navigationId: thirdSoftLCP.navigationId, name: 'soft-lcp (entry)'});

  const navigationHistory =
      (await dp.Page.getNavigationHistory()).result.entries;

  // Go back to the first hard navigation (text-lcp.html).
  dp.Page.navigateToHistoryEntry({entryId: navigationHistory.at(-6).id});
  const bfCacheRestore = await firstEntryAfter(
      firstHardNav.startTime + 1, 'back-forward-cache-restoration');
  perfEntries.push({
    navigationId: bfCacheRestore.navigationId,
    name: 'back-forward-cache-restoration (entry for text-lcp.html)'
  });

  // Add a big image to the page, which triggers an LCP.
  session.evaluate(function() {
    const img = new Image();
    img.src =
        'http://127.0.0.1:8000/inspector-protocol/resources/big-image.png';
    document.body.appendChild(img);
  });
  const lcpAfterBfCacheRestore = await firstEntryAfter(
      bfCacheRestore.startTime + 1, 'largest-contentful-paint');
  perfEntries.push({
    navigationId: lcpAfterBfCacheRestore.navigationId,
    name: 'hard-lcp (entry)'
  });

  const unfilteredEvents = await tracingHelper.stopTracing();

  const traceEntries = [];
  for (const event of unfilteredEvents.sort((a, b) => a.ts - b.ts)) {
    if (event.name === 'largestContentfulPaint::CandidateForSoftNavigation') {
      traceEntries.push({
        navigationId: event.args.data.performanceTimelineNavigationId,
        name: 'LCP candidate for soft navigation (trace)'
      });
    } else if (event.name === 'largestContentfulPaint::Candidate') {
      traceEntries.push({
        navigationId: event.args.data.performanceTimelineNavigationId,
        name: 'LCP candidate (trace)'
      });
    } else if (
      event.name === 'SoftNavigationStart') {
      traceEntries.push({
        navigationId: event.args.context.performanceTimelineNavigationId,
        name: 'Soft navigation (trace)'
      });
    }
  }

  // Maps actual IDs (e.g. navigationId) to logical IDs (e.g. 'id_0').
  class IdMapper {
    constructor() {
      this.logicalIdByActualId = new Map();
      this.actualIdByLogicalId = new Map();
    }

    map(id) {
      let logical = this.logicalIdByActualId.get(id);
      if (!logical) {
        logical = 'id_' + this.logicalIdByActualId.size;
        this.logicalIdByActualId.set(id, logical);
        this.actualIdByLogicalId.set(logical, id);
      }
      return logical;
    }

    reverseMap(logicalId) {
      return this.actualIdByLogicalId.get(logicalId);
    }
  }
  const ids = new IdMapper();

  testRunner.log(
      'Trace events and performance entries can be joined by navigationId.');
  while (perfEntries.length > 0 || traceEntries.length > 0) {
    const entries = [];
    const navigationId = perfEntries.length > 0 ? perfEntries[0].navigationId :
                                                  traceEntries[0].navigationId;
    while (perfEntries.length > 0 &&
           perfEntries[0].navigationId === navigationId) {
      entries.push(perfEntries.shift());
    }
    while (traceEntries.length > 0 &&
           traceEntries[0].navigationId === navigationId) {
      entries.push(traceEntries.shift());
    }
    for (const {navigationId, name} of entries) {
      testRunner.log('-> ' + ids.map(navigationId) + ' ' + name);
    }
    testRunner.log('');
  }

  testRunner.log(
      'Soft navigations and bfcache restores cause small positive navigationId increments:');
  for (const [first, second] of [
           ['id_2', 'id_3'], ['id_3', 'id_4'], ['id_4', 'id_5'],
           ['id_0', 'id_6']]) {
    const firstNavigationId = ids.reverseMap(first);
    const secondNavigationId = ids.reverseMap(second);
    const increment = secondNavigationId - firstNavigationId;
    testRunner.log('between ' + first + ' and ' + second + ':');
    testRunner.log(
        '-> increment is ' + (increment > 0 ? 'positive' : '0 or negative'));
    testRunner.log('-> increment is ' + (increment < 10 ? 'small' : 'large'));
  }

  testRunner.completeTest();
})

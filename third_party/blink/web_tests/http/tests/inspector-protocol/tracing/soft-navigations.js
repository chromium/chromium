(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests trace events for soft navigations.');

  // Check if the browser supports soft-navigation.
  testRunner.log(
      '\nPerformanceObserver supports "soft-navigation": ' +
      await session.evaluate(
          `PerformanceObserver.supportedEntryTypes.includes('soft-navigation')`));

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

  // Start tracing and observe the devtools.timeline category.
  await tracingHelper.startTracing('devtools.timeline');

  // Load test page and wait for the LCP element (it's also the click target).
  const url =
      'http://127.0.0.1:8000/inspector-protocol/resources/soft-navigations.html';
  await dp.Page.navigate({url});


  const lcpElementId = await session.evaluateAsync(function() {
    return new Promise(resolve => {
      new PerformanceObserver(list => {
        resolve(list.getEntries()[0].id);
      }).observe({type: 'largest-contentful-paint', buffered: true});
    })
  });
  testRunner.log('\nWaited for LCP element with id=' + lcpElementId);

  // The click which causes the soft navigation (via its click handler).
  userClick('click-target');

  // Use the PerformanceObserver to wait for both the soft navigation and the
  // soft LCP element.
  const [softNavigationName, softLcpElement] =
      await session.evaluateAsync(function() {
        return Promise.all([
          new Promise(resolve => {
            new PerformanceObserver(list => {
              resolve(list.getEntries()[0].name);
            }).observe({type: 'soft-navigation', buffered: true});
          }),
          new Promise(resolve => {
            new PerformanceObserver(list => {
              resolve(list.getEntries()[0].element.innerHTML);
            }).observe({
              type: 'interaction-contentful-paint',
              buffered: true
            });
          })
        ]);
      });
  testRunner.log(
      '\nGot soft navigation performance entry: ' + softNavigationName);
  testRunner.log('Got soft LCP element: ' + softLcpElement);

  // Stop tracing and log the SoftNavigation event.
  testRunner.log('\nStopping tracing and analyzing events.');
  let unfilteredEvents = await tracingHelper.stopTracing();

  // Filter + sort to the set of events we care about.
  // Primary sort: by timestamp.
  // Secondary sort: by event name, to maintain a consistent order for events
  // with the same timestamp. Note: SoftNavigationEntry TRACE macro is called
  // first, but the LCP entry is serialized first. Both use the same
  // presentation time value to mark the timestamp, explicitly.
  const supportedTraceEventNames = [
    'SoftNavigationStart',
    'largestContentfulPaint::Candidate',
    'largestContentfulPaint::CandidateForSoftNavigation',
  ];

  let filteredEvents =
      unfilteredEvents
          .filter(event => supportedTraceEventNames.includes(event.name))
          .sort((a, b) => {
            if (a.ts !== b.ts) {
              return a.ts - b.ts;
            }
            return supportedTraceEventNames.indexOf(a.name) -
                supportedTraceEventNames.indexOf(b.name);
          });


  // Maps timestamps (monotonically increasing double) to a counter.
  class TimestampMapper {
    constructor() {
      this.time = 0;
      this.counter = 0;
    }

    map(timestamp) {
      if (this.time === timestamp) {
        return this.counter;
      }
      if (this.time > timestamp) {
        throw new Error('Timestamps not in chronological order');
      }
      this.time = timestamp;
      this.counter++;
      return this.counter;
    }
  }

  // Maps actual IDs (e.g. navigationId) to logical IDs (e.g. 'id_0').
  class IdMapper {
    constructor() {
      this.logicalIdByActualId = new Map();
    }

    map(id) {
      let logical = this.logicalIdByActualId.get(id);
      if (!logical) {
        logical = 'id_' + this.logicalIdByActualId.size;
        this.logicalIdByActualId.set(id, logical);
      }
      return logical;
    }
  }

  const timestamps = new TimestampMapper();
  const ids = new IdMapper();
  const softNavs = [];
  const lcpCandidates = [];
  const lcpCandidatesForSoftNav = []
  for (const event of filteredEvents) {
    if (event.name === 'SoftNavigationStart') {
      testRunner.log('-> SoftNavigation event');
      testRunner.log(
        '   timeOrigin: ' +
        timestamps.map(event.args.context.timeOrigin));
      testRunner.log('   ts: ' + timestamps.map(event.ts));
      testRunner.log(
          '   firstContentfulPaint: ' +
          timestamps.map(event.args.context.firstContentfulPaint));
      testRunner.log('   frame: ' + ids.map(event.args.frame));
      testRunner.log(
          '   performanceTimelineNavigationId: ' +
          ids.map(event.args.context.performanceTimelineNavigationId));
      testRunner.log('   URL: ' + event.args.context.URL)
      softNavs.push(event);
    } else if (event.name === 'largestContentfulPaint::CandidateForSoftNavigation') {
      testRunner.log('-> LCP candidate for soft navigation event');
      testRunner.log('   ts: ' + timestamps.map(event.ts));
      testRunner.log('   frame: ' + ids.map(event.args.frame));
      testRunner.log(
          '   performanceTimelineNavigationId: ' +
          ids.map(event.args.data.performanceTimelineNavigationId));
      lcpCandidatesForSoftNav.push(event);
    } else if (event.name === 'largestContentfulPaint::Candidate') {
      testRunner.log('-> LCP candidate event');
      testRunner.log('   ts: ' + timestamps.map(event.ts));
      testRunner.log('   frame: ' + ids.map(event.args.frame));
      testRunner.log(
          '   performanceTimelineNavigationId: ' +
          ids.map(event.args.data.performanceTimelineNavigationId));
      lcpCandidates.push(event);
    }
  }

  testRunner.log('\nSoftNavigation event shape:');
  tracingHelper.logEventShape(softNavs[0]);

  testRunner.log('\nLCP candidate event shape:');
  tracingHelper.logEventShape(lcpCandidates[0]);

  testRunner.log('\nLCP candidate for soft navigation event shape:');
  tracingHelper.logEventShape(lcpCandidatesForSoftNav[0]);

  testRunner.completeTest();
})

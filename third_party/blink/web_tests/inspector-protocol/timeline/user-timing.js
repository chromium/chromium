(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(
      `
      <head></head>
      <body>
      </body>
  `,
      'Tests user timing events in traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("blink.user_timing");
  dp.Network.enable();

  // To be evaluated in page.
  async function pageFunction() {
    // Marks
    const zeroMark = performance.mark('~zero');
    const zero = zeroMark.startTime;

    performance.mark('@1500', {startTime: zero + 1500, detail: {aProperty:'This is a property'}});
    performance.mark('@1200', {startTime: zero + 1200});
    performance.mark('@1000', {startTime: zero + 1000});
    performance.mark('@500', {startTime: zero + 500});

    // Measures
    performance.measure('~zero to @500', '~zero', '@500');
    performance.measure(
        '@1000 to @1200',
        {detail: 'its @1000 to @1200', start: '@1000', end: '@1200'});
    performance.measure('@2000 for 300', {end: zero + 2300, duration: 300});
    // On the timeline the clearMarks() and clearMeasures() happen at time |zeroMark|.
    performance.clearMarks('@1000');
    performance.measure(
        '@1500 for 200',
        {detail: {key: 'val', num: 123}, start: '@1500', end: zero + 1700});
    performance.clearMeasures('@1500 for 200');
    performance.clearMeasures();
  }

  await session.evaluateAsync(`(${pageFunction.toString()})();`);

  const events = await tracingHelper.stopTracing(/blink.user_timing/);
  const sorted_events = events.sort((a, b) => {
    return (a['ts'] - b['ts']) || (a['ph']).localeCompare(b['ph'], 'en-US');
  });

  testRunner.log(
      '\nBelow offsets and startTimes are rounded to avoid flakiness on the bots.');
  testRunner.log(
      'FYI: startTime in args is the equivalent DOMHighResTimeStamp.');


  // Round ms to hundreds place
  const roundForFlakes = ms => {
    return Math.round(ms / 100) * 100;
  };

  const instantPhases = ['I', 'R'];
  const zeroMarkEvent = sorted_events.find(e => e.name === '~zero');
  const zeroTs = zeroMarkEvent.ts;
  const zeroHighRes = zeroMarkEvent.args.data.startTime;

  // Log a user timing trace event, normalized to avoid flakiness
  const logUserTimingEvent = e => {
    if (e.args.data?.navigationId) {
      e.args.data.navigationId = 'xNavIdx';
    }
    if (e.args.data?.startTime) {
      e.args.data.startTime =
          roundForFlakes(e.args.data.startTime - zeroHighRes);
    }
    if (e.args.startTime) {
      e.args.startTime = roundForFlakes(e.args.startTime - zeroHighRes);
    }

    const line = [
      e.ph.padEnd(2),
      e['name'].padEnd(20),
      (roundForFlakes((e.ts - zeroTs) / 1000)).toString().padStart(13),
      'ms  ',
      JSON.stringify(e.args),
    ].join(' ');
    return testRunner.log(line);
  };


  testRunner.log(`\nMarks:\nph ${'name'.padEnd(19)} offset from ~zero   args`);
  const instantEvents = sorted_events.filter(e => instantPhases.includes(e.ph));
  for (let e of instantEvents) {
    logUserTimingEvent(e);
  }
  testRunner.log(
      `\nMeasures:\nph ${'name'.padEnd(19)} offset from ~zero   args`);
  for (let e of sorted_events.filter(e => !instantEvents.includes(e))) {
    logUserTimingEvent(e);
  }

  testRunner.completeTest();
})

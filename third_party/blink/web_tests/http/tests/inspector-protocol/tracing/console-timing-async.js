(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests creating events with console.time and console.timeEnd');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  const Phase = TracingHelper.Phase;
  await dp.Page.enable();
  await tracingHelper.startTracing(
      'devtools.timeline,blink.console,blink.user_timing');

  // Generate console events that should look like so:
  // ================ console-label1
  //     ==================== console-label2
  await session.evaluateAsync(`
      (async function () {
        function delay(ms) {
          return new Promise((r) => setTimeout(r, ms));
        }
        console.time("console-label1");
        await delay(50);
        console.time("console-label2");
        await delay(50);
        console.timeEnd("console-label1");
        await delay(50);
        console.timeEnd("console-label2");
      })();
  `)

  const allEvents = await tracingHelper.stopTracing(
      /devtools\.timeline|blink\.console|blink\.user_timing/);

  const consoleTimeEvents =
      allEvents.filter(event => event.name.startsWith('console-')).sort((a, b) => a.ts - b.ts)

  testRunner.log('Got ConsoleTime events:');
  for(event of consoleTimeEvents) {
    testRunner.log({
        name: event.name,
        ph: event.ph,
    });
  }
  for(let i = 0; i < consoleTimeEvents.length - 1; i++) {
        const current = consoleTimeEvents[i];
        const next = consoleTimeEvents[i+1];
        testRunner.log(`Event ${i} (${current.name} ${current.ph}) ts is less than event ${i+1} (${next.name} ${next.ph}): ${current.ts < next.ts}`)
        if(current.ts === next.ts) {
            testRunner.log(`Events (${current.name} ${current.ph}) and (${next.name} ${next.ph}) have the same timestamp ${current.ts}`)
        }
    }
  testRunner.completeTest();
})

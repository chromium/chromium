(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests reporting of unused preloads in traces.');

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await dp.Page.enable();
  await dp.Network.enable();

  await tracingHelper.startTracing('blink.resource,devtools.timeline');
  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/unused-preload.html'});

  await dp.Page.onceLoadEventFired();
  // We have to wait for a period of time so that the UnusedPreload event is
  // emitted. This is emitted 3 seconds after the onload event, so we wait for
  // 3.5 to allow for slight variance.
  await new Promise(r => setTimeout(r, 3_500));

  const events = await tracingHelper.stopTracing(/blink.resource|devtools.timeline/);

  const networkEvent = events.find(event => {
    return event.name === 'ResourceSendRequest' && event.args.data.url.includes('empty.js');
  })
  const unusedPreloadEvent = events.find(event => event.name === 'ResourceFetcher::WarnUnusedPreloads');

  if(!networkEvent) {
    testRunner.log('Could not find network event for empty.js.');
  }

  if(!unusedPreloadEvent) {
    testRunner.log('Could not find unused-preload warning event.');
  }

  function truncate(url) {
    const pathname = new URL(url).pathname;
    const path_array = pathname.split('/');
    return path_array[path_array.length - 1];
  }

  const warningIsForEmptyJSRequest = networkEvent.args.data.requestId === unusedPreloadEvent.args.data.requestId;
  testRunner.log(`Preload warning is for the expected empty.js request: ${warningIsForEmptyJSRequest}`);
  testRunner.completeTest();
});

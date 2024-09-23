(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Network request lifecycle trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Network.enable();

  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

  testRunner.log('Testing XHR request with responseType of \'text\'');

  function logResultOutput(events) {
    const stateChangeEvents =
        events.filter(event => event.name === 'XHRReadyStateChange');

    testRunner.log(
        `Found ${stateChangeEvents.length} XHRReadyStateChange events:`);
    stateChangeEvents.forEach((event, index) => {
      testRunner.log(`Event ${index + 1}:`);
      testRunner.log(`=> readyState: ${event.args.data.readyState}`);
      testRunner.log(`=> url: ${event.args.data.url}\n`);
    })
    const loadEvent = events.find(event => event.name === 'XHRLoad');
    if (!loadEvent) {
      testRunner.log('ERROR: did not find XHRLoad event.');
    }
    testRunner.log(`XHRLoad event URL: ${loadEvent.args.data.url}`);
  }

  await tracingHelper.invokeAsyncWithTracing(function() {
    let callback;
    let promise = new Promise((fulfill) => callback = fulfill);
    let xhr = new XMLHttpRequest();
    xhr.open('GET', 'blank.js', true);
    xhr.onload = callback;  // This is necessary for XHRLoad event.
    xhr.onreadystatechange = function() {};  // This is necessary for XHRReadyStateChange event.
    xhr.send(null);
    return promise;
  });
  const xhrTimelineEvents = tracingHelper.filterEvents((event) => {
    return event.name.startsWith('XHR');
  });
  logResultOutput(xhrTimelineEvents);

  testRunner.log('\n\nTesting XHR request with responseType of \'blob\'');
  await tracingHelper.invokeAsyncWithTracing(function() {
    let callback;
    let promise = new Promise((fulfill) => callback = fulfill);
    let xhr = new XMLHttpRequest();
    xhr.responseType = 'blob';
    xhr.open('GET', 'blank.js', true);
    xhr.onload = function() {};  // This is necessary for XHRLoad event.
    xhr.onreadystatechange = done;
    function done() {
      if (xhr.readyState === 4) {
        callback();
      }
    }
    xhr.send(null);
    return promise;
  });
  const xhrBlobTimelineEvents = tracingHelper.filterEvents((event) => {
    return event.name.startsWith('XHR');
  });
  logResultOutput(xhrBlobTimelineEvents);

  testRunner.completeTest();
});

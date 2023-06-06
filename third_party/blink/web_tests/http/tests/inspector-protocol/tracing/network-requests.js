(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Network request lifecycle trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();

  let resourceSendRequest;
  let resourceWillSendRequest;
  let resourceReceiveResponse;
  let resourceReceivedData;
  let resourceFinish;

  const MAX_ATTEMPTS = 3;
  for (let attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
    await tracingHelper.startTracing('devtools.timeline');
    dp.Page.navigate(
        {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

    // Wait for the DOM to be interactive.
    await dp.Page.onceLoadEventFired();

    const timelineEvents = await tracingHelper.stopTracing(/devtools.timeline/);

    // Find and test all events of the lifecycle of the request for the
    // HTML document.
    resourceSendRequest = timelineEvents.find(
        event => event.name === 'ResourceSendRequest' &&
            event.args.data.url.includes('basic.html'));
    resourceWillSendRequest = timelineEvents.find(
        event =>
            eventBelongsToDocumentRequest(event, 'ResourceWillSendRequest'));
    resourceReceiveResponse = timelineEvents.find(
        event =>
            eventBelongsToDocumentRequest(event, 'ResourceReceiveResponse'));
    resourceReceivedData = timelineEvents.find(
        event => eventBelongsToDocumentRequest(event, 'ResourceReceivedData'));
    resourceFinish = timelineEvents.find(
        event => eventBelongsToDocumentRequest(event, 'ResourceFinish'));

    if (resourceSendRequest && resourceWillSendRequest &&
        resourceReceiveResponse && resourceReceivedData && resourceFinish) {
      break;
    }
  }

  testRunner.log('Got ResourceWillSendRequest event:');
  tracingHelper.logEventShape(resourceWillSendRequest)

  testRunner.log('Got ResourceSendRequest event:');
  tracingHelper.logEventShape(resourceSendRequest)

  testRunner.log('Got ResourceReceiveResponse event:');
  tracingHelper.logEventShape(resourceReceiveResponse)

  testRunner.log('Got ResourceReceivedData event:');
  tracingHelper.logEventShape(resourceReceivedData)

  testRunner.log('Got ResourceFinish event:');
  tracingHelper.logEventShape(resourceFinish)

  testRunner.completeTest();

  function eventBelongsToDocumentRequest(event, expectedName) {
    return event.name === expectedName &&
        event.args.data.requestId === resourceSendRequest.args.data.requestId;
  }
});

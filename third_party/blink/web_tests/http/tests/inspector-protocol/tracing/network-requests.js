(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Network request lifecycle trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('devtools.timeline');
  await dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});


  // Wait for trace events.
  await new Promise(resolve => setTimeout(resolve, 1000));

  await tracingHelper.stopTracing(/devtools.timeline/);


  const resourceWillSendRequest =
      tracingHelper.findEvent('ResourceWillSendRequest', 'I');
  const resourceSendRequest =
      tracingHelper.findEvent('ResourceSendRequest', 'I');
  const resourceReceiveResponse =
      tracingHelper.findEvent('ResourceReceiveResponse', 'I');
  const resourceReceivedData =
      tracingHelper.findEvent('ResourceReceivedData', 'I');
  const resourceFinish = tracingHelper.findEvent('ResourceFinish', 'I');


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

  const allRequestIds = [
    resourceWillSendRequest, resourceSendRequest, resourceReceiveResponse,
    resourceReceivedData, resourceFinish
  ].map(event => event.args.data.requestId);

  const allIdsAreEqual = allRequestIds.every(id => id === allRequestIds[0]);

  if (allIdsAreEqual) {
    testRunner.log('All request ids are equal');
  }

  testRunner.completeTest();
})

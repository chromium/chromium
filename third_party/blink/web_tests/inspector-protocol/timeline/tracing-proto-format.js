(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that tracing with proto format outputs something resembling protos.`);

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  const startResponse = await dp.Tracing.start(
      {transferMode: 'ReturnAsStream', streamFormat: 'proto'});
  if (startResponse.error) {
    testRunner.log('Start failed: ' + startResponse.error.message);
    testRunner.completeTest();
    return;
  }

  const stream = await tracingHelper.stopTracingAndReturnStream();
  const data = await tracingHelper.retrieveStream(stream, null, null);
  // First byte should be TracePacket field ID preamble (byte value 10).
  testRunner.log('First byte: ' + data.charCodeAt(0));
  testRunner.completeTest();
})

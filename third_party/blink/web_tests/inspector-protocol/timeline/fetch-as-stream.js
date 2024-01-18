(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
    div#test {
        display: none;
        background-color: blue;
        width: 100px;
        height: 100px;
    }
    </style>
    <div id='test'>
    </div>
  `, 'Tests fetching trace through IO domain stream.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracingAndSaveAsStream();
  await session.evaluate(`
    (function performActions() {
      var element = document.getElementById('test');
      element.style.display = 'block';
      var unused = element.clientWidth;
    })();
  `);

  var streamHandle = await tracingHelper.stopTracingAndReturnStream();
  var data1 = await tracingHelper.retrieveStream(streamHandle, null, null);
  var data2 = await tracingHelper.retrieveStream(streamHandle, 0, 1000);
  if (data1 !== data2)
    testRunner.log('FAIL: got different data for cunked vs. non-chunked reads');
  var response = await dp.IO.close({ handle: streamHandle });
  testRunner.log('Error after legit close: ' + JSON.stringify(response.error));
  response = await dp.IO.read({ handle: streamHandle });
  testRunner.log('Error after illegal read: ' + JSON.stringify(response.error));
  response = await dp.IO.close({ handle: streamHandle });
  testRunner.log('Error after illegal close: ' + JSON.stringify(response.error));
  var trace = JSON.parse(data1);
  performEventsSanityCheck(trace['traceEvents']);
  testRunner.log('Metadata: ' + typeof trace['metadata'] + (trace['metadata'] ? ', not null' : ''));
  testRunner.completeTest();

  function assertGreaterOrEqual(a, b, message) {
    if (a >= b)
      return;
    testRunner.log(message + ' (' + a + ' < ' + b + ')');
    testRunner.completeTest();
  }

  function performEventsSanityCheck(events) {
    var phaseComplete = 0;

    var knownEvents = {
      'MessageLoop::PostTask': 0,
      'FunctionCall': 0,
      'UpdateLayoutTree': 0,
      'Layout': 0
    };

    for (var i = 0; i < events.length; ++i) {
      var event = events[i];
      if (event.phase === 'X')
        ++phaseComplete;
      if (event.name in knownEvents)
        ++knownEvents[event.name];
    }
    assertGreaterOrEqual(events.length, 10, 'Too few trace events recorded');
    assertGreaterOrEqual(knownEvents['UpdateLayoutTree'], 1, 'Too few UpdateLayoutTree events');
    assertGreaterOrEqual(knownEvents['Layout'], 1, 'Too few Layout events');
    testRunner.log('Event sanity test done');
  }
})

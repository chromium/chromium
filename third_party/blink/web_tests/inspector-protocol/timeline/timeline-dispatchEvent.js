(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='my-div'></div>
  `, 'Tests trace events for event dispatching.');

  function performAction() {
    var div = document.querySelector('#my-div');
    div.addEventListener('click', function(e) {  }, false);
    div.click();

    var iframe = document.createElement('iframe');
    div.appendChild(iframe);
    return new Promise(resolve => {
      iframe.onload = resolve;
      iframe.src = 'blank.html';
    });
  }

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.invokeAsyncWithTracing(performAction);

  var windowEventNames = [ 'click', 'beforeunload', 'unload', 'load' ];
  for (var eventName of windowEventNames) {
    var events = tracingHelper.filterEvents(e => e.name === 'EventDispatch' && e.args.data.type === eventName);
    if (events.length >= 1)
      testRunner.log('SUCCESS: found ' + eventName + ' event');
    else
      testRunner.log('FAIL: ' + eventName + ' event is missing; devtools.timeline events: ' + tracingHelper.formattedEvents());
  }

  testRunner.completeTest();
})

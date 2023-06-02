(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the Timeline API instrumentation of a TimerFired events inside evaluated scripts.');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await tracingHelper.startTracing('devtools.timeline');

  await session.evaluateAsync(`
(function performActions()
{
    var promise = new Promise((fulfill) => window.callWhenDone = fulfill);
    var content = "" +
        "var fn2 = function() {" +
        "    console.timeStamp(\\"Script evaluated\\");" +
        "    window.callWhenDone();" +
        "};\\\\n" +
        "var fn1 = function() {" +
        "    window.setTimeout(fn2, 1);" +
        "};\\\\n" +
        "window.setTimeout(fn1, 1);\\\\n" +
        "//# sourceURL=fromEval.js";
    content = "eval('" + content + "');";
    var scriptElement = document.createElement('script');
    var contentNode = document.createTextNode(content);
    scriptElement.appendChild(contentNode);
    document.body.appendChild(scriptElement);
    document.body.removeChild(scriptElement);
    return promise;
})()
`);
  const devtoolsEvents = await tracingHelper.stopTracing(/devtools\.timeline/);

  for (let i = 0; i < devtoolsEvents.length; ++i) {
    if (devtoolsEvents[i].name !== 'TimerFire')
      continue;

    const timerFireEventEndTime = devtoolsEvents[i].ts + devtoolsEvents[i].dur;
    for (let j = i + 1; j < devtoolsEvents.length; ++j) {
      const event = devtoolsEvents[j];
      const eventEndTime = event.ts + event.dur;
      if (eventEndTime > timerFireEventEndTime) {
        break;
      }
      if (event.name === 'FunctionCall') {
        const fnCallSite = event.args['data'];
        testRunner.log(`${devtoolsEvents[i].name} ${fnCallSite.url}:${
            fnCallSite.lineNumber}`);
      }
    }
  }
  testRunner.completeTest();
});

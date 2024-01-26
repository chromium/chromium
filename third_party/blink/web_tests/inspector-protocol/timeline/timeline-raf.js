(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='myDiv'>DIV</div>
  `, 'Tests trace events for rafs.');

  function performActions() {
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    var rafId2;
    var rafId1 = requestAnimationFrame(() => callback({ rafId1: rafId1, rafId2: rafId2 }));
    rafId2 = requestAnimationFrame(function() { });
    cancelAnimationFrame(rafId2);
    return promise;
  }

  function hasRafId(id, e) {
    return e.args.data.id === id;
  }

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  var data = await tracingHelper.invokeAsyncWithTracing(performActions);
  var firedRaf = data.rafId1;
  var canceledRaf = data.rafId2;

  var raf1 = tracingHelper.findEvent('RequestAnimationFrame', 'I', hasRafId.bind(null, firedRaf));
  var raf2 = tracingHelper.findEvent('RequestAnimationFrame', 'I', hasRafId.bind(null, canceledRaf));

  testRunner.log('RequestAnimationFrame has frame: ' + !!raf1.args.data.frame);
  testRunner.log('RequestAnimationFrame frames match: ' + (raf1.args.data.frame === raf2.args.data.frame));

  tracingHelper.findEvent('CancelAnimationFrame', 'I', hasRafId.bind(null, canceledRaf));
  tracingHelper.findEvent('FireAnimationFrame', 'X', hasRafId.bind(null, firedRaf));

  testRunner.log('SUCCESS: found all expected events.');
  testRunner.completeTest();
})

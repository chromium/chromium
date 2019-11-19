(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
    .my-class {
        min-width: 100px;
        background-color: red;
    }
    </style>
    <div id='myDiv'>DIV</div>
  `, 'Tests trace events for layout.');

  function performActions() {
    var div = document.querySelector('#myDiv');
    div.classList.add('my-class');
    div.offsetWidth;
    return Promise.resolve();
  }

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.invokeAsyncWithTracing(performActions);

  var schedRecalc = tracingHelper.findEvent('ScheduleStyleRecalculation', 'I');
  var recalc = tracingHelper.findEvent('UpdateLayoutTree', 'X');
  testRunner.log('UpdateLayoutTree frames match: ' + (schedRecalc.args.data.frame === recalc.args.beginData.frame));
  testRunner.log('UpdateLayoutTree elementCount > 0: ' + (recalc.args.elementCount > 0));

  var invalidate = tracingHelper.findEvent('InvalidateLayout', 'I');
  var layout = tracingHelper.findEvent('Layout', 'X');

  testRunner.log('InvalidateLayout frames match: ' + (recalc.args.beginData.frame === invalidate.args.data.frame));

  var beginData = layout.args.beginData;
  testRunner.log('Layout frames match: ' + (invalidate.args.data.frame === beginData.frame));
  testRunner.log('dirtyObjects > 0: ' + (beginData.dirtyObjects > 0));
  testRunner.log('totalObjects > 0: ' + (beginData.totalObjects > 0));

  var endData = layout.args.endData;
  testRunner.log('has rootNode id: ' + (endData.rootNode > 0));
  testRunner.log('has root quad: ' + !!endData.root);

  testRunner.log('SUCCESS: found all expected events.');
  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
      @keyframes anim {
        0% {width: 100px;},
        100% {width: 200px;},
      }
    </style>
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests animation started and records composite failure reasons in trace event.');
  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);

  dp.Animation.enable();
  await tracingHelper.startTracing();

  session.evaluate(`
    requestAnimationFrame(() => {
      node.style.animation = 'anim 2s';
    })
  `)
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  session.evaluate(`
    requestAnimationFrame(() => {
      node.style.animation = '';
    });
  `)
  await dp.Animation.onceAnimationCanceled();

  await tracingHelper.stopTracing();

  var animationEvents = tracingHelper.filterEvents(e => e.name === 'Animation');
  animationEvents.forEach(e => testRunner.log(`Name:${e.name},Phase:${e.ph}`));
  var animation = animationEvents[1];
  var properties = animation.args.data.unsupportedProperties;
  testRunner.log('Animation composite failed reasons: ' + animation.args.data.compositeFailed);
  for (const p of properties) {
    testRunner.log('Unsupported CSS Property: ' + p);
  }

  testRunner.completeTest();
})

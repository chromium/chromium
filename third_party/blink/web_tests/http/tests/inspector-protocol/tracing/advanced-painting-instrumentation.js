(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of advanced painting instrumentation trace events');

  // Fail with a pointer to the pending await rather than an opaque harness
  // timeout. Kept comfortably below the harness timeout of 30s.
  let errorForLog = new Error()
  setTimeout(() => {
    testRunner.die('Took longer than 20s', errorForLog);
  }, 20000);

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  errorForLog = new Error()
  await tracingHelper.startTracing(
      'disabled-by-default-cc.debug,disabled-by-default-viz.quads,disabled-by-default-devtools.timeline.layers,disabled-by-default-devtools.timeline.picture');
  errorForLog = new Error()

  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/resources/iframe-navigation.html'
  });

  // Wait for the DOM to be interactive.
  await dp.Page.onceLoadEventFired();
  errorForLog = new Error()

  await tracingHelper.stopTracing(
      /disabled-by-default-(devtools\.timeline\.)?(cc\.debug|layers|picture)/);

  errorForLog = new Error()

  const LayerTreeHostImpl = tracingHelper.findEvent(
      'LayerTreeHostImpl:snapshot', TracingHelper.Phase.INSTANT);
  const DisplayItemList = tracingHelper.findEvent(
      'cc::DisplayItemList:snapshot', TracingHelper.Phase.INSTANT);

  // The snapshot payload is excluded from the shape: it contains layer and
  // tile arrays whose length varies between runs. Its fields are asserted
  // individually below instead. The name and phase are logged as values
  // rather than types because they are the contract consumers depend on.
  const excludeSnapshot = ['snapshot'];
  const exposeFormat = ['name', 'ph'];

  testRunner.log('Got a LayerTreeHostImpl Event:');
  tracingHelper.logEventShape(LayerTreeHostImpl, excludeSnapshot, exposeFormat);
  testRunner.log(`type of device_viewport_size: ${
      typeof LayerTreeHostImpl.args.snapshot.device_viewport_size}`);
  testRunner.log(`type of layers: ${
      typeof LayerTreeHostImpl.args.snapshot.active_tree.layers}`);

  testRunner.log('Got a DisplayItemList Event');
  tracingHelper.logEventShape(DisplayItemList, excludeSnapshot, exposeFormat);
  testRunner.log(
      `type of params: ${typeof DisplayItemList.args.snapshot.params}`);
  testRunner.log(`type of layer_rect: ${
      typeof DisplayItemList.args.snapshot.params.layer_rect}`);
  testRunner.log(
      `type of skp64: ${typeof DisplayItemList.args.snapshot.skp64}`);

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {_, session, dp} = await testRunner.startBlank(`Test graph events for the object connection.`);

  await dp.WebAudio.enable();

  function anonymizeUuid(uuid) {
    return (typeof uuid === 'string' && uuid.length === 36) ? '<GraphObjectId>' : null;
  }

  function logEvent(event) {
    testRunner.log(`EventType = ${event.method}`);
    testRunner.log(` contextId : ${anonymizeUuid(event.params.contextId)}`);
    testRunner.log(` sourceId : ${anonymizeUuid(event.params.sourceId)}`);
    testRunner.log(` destinationId : ${anonymizeUuid(event.params.destinationId)}`);
    testRunner.log(` sourceOutputIndex : ${event.params.sourceOutputIndex}`);
    testRunner.log(` destinationInputIndex : ${event.params.destinationInputIndex}`);
  }

  // AudioNode-AudioNode connection without specified indices.
  session.evaluate(`
    const context = new AudioContext();
    const gain1 = new GainNode(context);
    const gain2 = new GainNode(context);
    gain1.connect(gain2);
  `);
  logEvent(await dp.WebAudio.onceNodesConnected());

  // AudioNode disconnection with a specified destination.
  session.evaluate(`
    gain1.disconnect(gain2);
  `);
  logEvent(await dp.WebAudio.onceNodesDisconnected());

  // AudioNode disconnection without a specified destination.
  session.evaluate(`
    gain2.connect(context.destination);
    gain2.disconnect();
  `);
  logEvent(await dp.WebAudio.onceNodesDisconnected());

  // AudioNode-AudioNode connection with specified input/output index.
  session.evaluate(`
    const splitter = new ChannelSplitterNode(context);
    const merger = new ChannelMergerNode(context);
    splitter.connect(merger, 1, 4);
  `);
  logEvent(await dp.WebAudio.onceNodesConnected());

  // AudioNode-AudioNode disconnection with specified input/output index.
  session.evaluate(`
    splitter.disconnect(merger, 1, 4);
  `);
  logEvent(await dp.WebAudio.onceNodesDisconnected());

  // AudioNode-AudioParam connection.
  session.evaluate(`
    const osc = new OscillatorNode(context);
    gain2.connect(osc.frequency);
  `);
  logEvent(await dp.WebAudio.onceNodeParamConnected());

  // AudioNode-AudioParam disconnection.
  session.evaluate(`
    gain2.disconnect(osc.frequency);
  `);
  logEvent(await dp.WebAudio.onceNodeParamDisconnected());

  // AudioNode.outputIndex-AudioParam connection.
  session.evaluate(`
    splitter.connect(osc.frequency, 1);
  `);
  logEvent(await dp.WebAudio.onceNodeParamConnected());

  // AudioNode.outputIndex-AudioParam disconnection.
  session.evaluate(`
    splitter.disconnect(osc.frequency, 1);
  `);
  logEvent(await dp.WebAudio.onceNodeParamDisconnected());

  await dp.WebAudio.disable();
  testRunner.completeTest();
});

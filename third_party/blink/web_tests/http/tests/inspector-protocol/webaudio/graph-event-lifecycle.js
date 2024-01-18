(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // Data structure for the comprehensive testing. Each AudioNode can have
  // an unique constructor pattern and also can have a different set of
  // AudioParams. Each data entry has:
  //   {string} nodeName AudioNode type
  //   {string=} ctorString JS code string for the node construction. This is
  //                        needed when the simple construction doesn't work.
  //   {Array<string>=} audioParams Names of the associated AudioParam objects
  const testData = [
    {
      nodeName: 'AnalyserNode',
    },
    {
      nodeName: 'AudioBufferSourceNode',
      audioParams: ['detune', 'playbackRate'],
    },
    {
      nodeName: 'AudioWorkletNode',
      ctorString: `
        const processorSource =
            'registerProcessor("test-processor", class extends AudioWorkletProcessor { process() {} });';
        const blob = new Blob([processorSource], {type: 'application/javascript'});
        const objectURL = URL.createObjectURL(blob);
        context.audioWorklet.addModule(objectURL).then(() => {
          new AudioWorkletNode(context, 'test-processor');
        });
      `,
    },
    {
      nodeName: 'BiquadFilterNode',
      audioParams: ['frequency', 'detune', 'Q', 'gain'],
    },
    {
      nodeName: 'ChannelMergerNode',
    },
    {
      nodeName: 'ChannelSplitterNode',
    },
    {
      nodeName: 'ConstantSourceNode',
      audioParams: ['offset'],
    },
    {
      nodeName: 'ConvolverNode',
    },
    {
      nodeName: 'DelayNode',
      audioParams: ['delayTime'],
    },
    {
      nodeName: 'DynamicsCompressorNode',
      audioParams: ['threshold', 'knee', 'ratio', 'attack', 'release'],
    },
    {
      nodeName: 'GainNode',
      audioParams: ['gain'],
    },
    {
      nodeName: 'IIRFilterNode',
      ctorString:
          `new IIRFilterNode(context, {feedforward: [1], feedback: [1, -0.99]});`,
    },
    {
      nodeName: 'MediaElementAudioSourceNode',
      ctorString: `
        const audioElement = new Audio();
        new MediaElementAudioSourceNode(context, {mediaElement: audioElement});
      `,
    },
    {
      nodeName: 'MediaStreamAudioDestinationNode',
      ctorString: `new MediaStreamAudioDestinationNode(context);`,
    },
    {
      nodeName: 'MediaStreamAudioSourceNode',
      ctorString: `
        const generator = new MediaStreamTrackGenerator({kind: 'audio'});
        const stream = new MediaStream([generator]);
        new MediaStreamAudioSourceNode(context, {mediaStream: stream});
      `,
    },
    {
      nodeName: 'OscillatorNode',
      audioParams: ['frequency', 'detune'],
    },
    {
      nodeName: 'PannerNode',
      audioParams: [
        'positionX', 'positionY', 'positionZ', 'orientationX', 'orientationY',
        'orientationZ'
      ],
    },
    {
      nodeName: 'ScriptProcessorNode',
      ctorString: `context.createScriptProcessor();`,
    },
    {
      nodeName: 'StereoPannerNode',
      audioParams: ['pan'],
    },
    {
      nodeName: 'WaveShaperNode',
    },
  ];

  const {_, session, dp} = await testRunner.startBlank(
      `Test graph events for the object lifecycle.`);

  await dp.WebAudio.enable();

  let event;

  // Create an AudioContext. A context contains pre-constructed
  // AudioDestinationNode and AudioListener. Note that AudioListener contains
  // 9 AudioParams.
  session.evaluate('const context = new AudioContext();');
  event = await dp.WebAudio.onceContextCreated();
  testRunner.log(`Successfully created: BaseAudioContext(${event.params.context.contextType})`);
  event = await dp.WebAudio.onceAudioNodeCreated();
  testRunner.log(`Successfully created: AudioDestinationNode`);
  event = await dp.WebAudio.onceAudioListenerCreated();
  testRunner.log(`Successfully created: AudioListener`);
  for (let i = 0; i < 9; ++i) {
    event = await dp.WebAudio.onceAudioParamCreated();
    testRunner.log(`  - AudioParam::${event.params.param.paramType}`);
  }

  for (const entry of testData) {
    session.evaluate(entry.ctorString || `new ${entry.nodeName}(context);`);
    event = await dp.WebAudio.onceAudioNodeCreated();
    testRunner.log(`Successfully created: ${event.params.node.nodeType}`);
    if (entry.audioParams) {
      for (let i = 0; i < entry.audioParams.length; ++i) {
        event = await dp.WebAudio.onceAudioParamCreated();
        testRunner.log(`  - AudioParam::${event.params.param.paramType}`);
      }
    }
  }

  // There is no way to invoke GC in the session, |fooWillBeDestroyed()| events
  // cannot be tested.

  await dp.WebAudio.disable();
  testRunner.completeTest();
});

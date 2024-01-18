
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startBlank(
        'Tests the data of CPU profiling trace events');
    const TracingHelper =
        await testRunner.loadScript('../resources/tracing-test.js');
    const Phase = TracingHelper.Phase;
    const tracingHelper = new TracingHelper(testRunner, session);
    await dp.Page.enable();
    await tracingHelper.startTracing(
        'v8,devtools.timeline,disabled-by-default-devtools.timeline,disabled-by-default-v8.cpu_profiler,disabled-by-default-v8.compile');
    dp.Page.navigate({
      url: 'http://127.0.0.1:8000/inspector-protocol/resources/cpu-profiling.html'
    });
    // Wait for the DOM to be interactive.
    await dp.Page.onceLoadEventFired();
    // Wait for trace events.
    await session.evaluateAsync(`window.__blockingHandlerPromise`);
    await session.evaluateAsync(`window.__modulePromise`);
    await session.evaluateAsync(`window.__asyncScriptPromise`);
    // Profiling events can take a while until they are dispatched.
    await new Promise(res => setTimeout(res, 1000));
    const allEvents = await tracingHelper.stopTracing(
        /(disabled-by-default-)?devtools\.timeline|v8\.cpu_profiler|v8\.compile|v8/);
    const compileScript = tracingHelper.findEvent('v8.compile', Phase.COMPLETE);
    const compileCode = tracingHelper.findEvent('V8.CompileCode', Phase.COMPLETE);
    const optimizeCode =
        tracingHelper.findEvent('V8.OptimizeCode', Phase.COMPLETE);
    const evaluateScript =
        tracingHelper.findEvent('EvaluateScript', Phase.COMPLETE);
    const cacheScript =
        tracingHelper.findEvent('v8.produceCache', Phase.COMPLETE);
    const compileModule =
        tracingHelper.findEvent('v8.compileModule', Phase.COMPLETE);
    const evaluateModule =
        tracingHelper.findEvent('v8.evaluateModule', Phase.COMPLETE);

    const cacheModule =
        tracingHelper.findEvent('v8.produceModuleCache', Phase.COMPLETE);

    const profile = tracingHelper.findEvent('Profile', Phase.SAMPLE);

    const profileChunk = allEvents.find(
        e => e.name === 'ProfileChunk' && e.args.data.cpuProfile &&
            e.args.data.cpuProfile.nodes);
    const parseOnBackground =
        tracingHelper.findEvent('v8.parseOnBackground', Phase.COMPLETE);

    const parseOnBackgroundParsing =
        tracingHelper.findEvent('v8.parseOnBackgroundParsing', Phase.COMPLETE);
    testRunner.log('Got a v8.compile event');
    tracingHelper.logEventShape(compileScript, ['notStreamedReason', 'streamed']);
    testRunner.log('Got a V8.CompileCode event');
    tracingHelper.logEventShape(compileCode);
    testRunner.log('Got a V8.OptimizeCode event');
    tracingHelper.logEventShape(optimizeCode);
    testRunner.log('Got an EvaluateScript event');
    tracingHelper.logEventShape(evaluateScript);
    testRunner.log('Got a v8.produceCache event');
    tracingHelper.logEventShape(cacheScript);
    testRunner.log('Got an v8.compileModule event');
    tracingHelper.logEventShape(compileModule, ['notStreamedReason', 'streamed']);
    testRunner.log('Got a v8.evaluateModule event');
    tracingHelper.logEventShape(evaluateModule);
    testRunner.log('Got a v8.produceModuleCache event');
    tracingHelper.logEventShape(cacheModule);
    testRunner.log('Got a Profile event');
    tracingHelper.logEventShape(profile);
    testRunner.log('Got a ProfileChunk event');
    testRunner.log('CPU profile has:');
    const cpuProfile = profileChunk.args.data.cpuProfile;
    testRunner.log('nodes:');
    tracingHelper.logEventShape(cpuProfile.nodes[0]);
    testRunner.log(`samples: ${typeof cpuProfile.samples[0]}`);
    testRunner.log(`timeDeltas: ${typeof profileChunk.args.data.timeDeltas[0]}`);
    testRunner.log('Got a v8.parseOnBackground event');
    tracingHelper.logEventShape(parseOnBackground)
    testRunner.log('Got a parseOnBackgroundParsing event');
    tracingHelper.logEventShape(parseOnBackgroundParsing)
    testRunner.completeTest();
  })

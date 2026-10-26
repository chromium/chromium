(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Test that prerender nested cross-origin iframe navigations receive status updates`);

  const childTargetManager =
      testRunner.createChildTargetManagerFor(tabTargetSession);
  await childTargetManager.startAutoAttach();
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  session1.navigate('resources/cross-origin-iframe-prerender-nesting.php');

  const eventsByPipelineId = new Map();
  const rawEvents = [];
  function recordEvent(event) {
    rawEvents.push(event);
    const pipelineId = event.params.pipelineId;
    if (!eventsByPipelineId.has(pipelineId)) {
      eventsByPipelineId.set(pipelineId, []);
    }
    eventsByPipelineId.get(pipelineId).push(event.params);
  }

  // 1. Top frame: Pending, Running, Ready (3 events)
  // 2. Middle iframe with SLM: Running, Ready (2 events)
  // 3. Nested same-origin iframe under SLM: Running, Ready (2 events)
  // 4. Nested cross-origin iframe under SLM: Running, Ready (2 events)
  // 5. Middle iframe without SLM: Running, Ready (2 events)
  // 6. Nested same-origin iframe under no-SLM: Running, Ready (2 events)
  // 7. Nested cross-origin iframe under no-SLM: Running, Failure (2 events)
  for (let i = 0; i < 15; ++i) {
    recordEvent(await dp1.Preload.oncePrerenderStatusUpdated());
  }

  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  // Activate prerendered page.
  session1.evaluate(`document.getElementById('link').click()`);

  // Success for top frame and 5 non-failed subframes (6 events).
  for (let i = 0; i < 6; ++i) {
    recordEvent(await dp2.Preload.oncePrerenderStatusUpdated());
  }

  const pipelineIds = [...eventsByPipelineId.keys()];
  for (let i = 0; i < pipelineIds.length; ++i) {
    const events = eventsByPipelineId.get(pipelineIds[i]);
    const url = events[0].key.url;
    for (const params of events) {
      if (params.key.url !== url) {
        testRunner.log(`URL mismatch for pipelineIdIndex ${i}: ${
            params.key.url} !== ${url}`);
      }
    }
    const lines = events.map(params => {
      if (params.prerenderStatus) {
        return `      status : ${params.status}, prerenderStatus : ${
            params.prerenderStatus}`;
      }
      return `      status : ${params.status},`;
    });
    testRunner.log(`{
    pipelineIdIndex:${i}
    url : ${url}
    Preload.prerenderStatusUpdated: {
${lines.join('\n')}
    }
}`);
  }

  testRunner.log('Raw devtools protocol:');
  for (const event of rawEvents) {
    event.params.pipelineIdIndex = pipelineIds.indexOf(event.params.pipelineId);
    testRunner.log(event);
  }

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Test that heap tracking actually reports data fragments.');

  await session.evaluate(`
    const junkArray = new Array(1000);
    function junkGenerator() {
      for (let i = 0; i < junkArray.length; ++i)
        junkArray[i] = '42 ' + i;
      window.junkArray = junkArray;
    }
    setInterval(junkGenerator, 0)
  `);

  let lastSeenObjectIdEventCount = 0;
  let heapStatsUpdateEventCount = 0;
  let fragments = [];

  const objectIdCallback = async messageObject => {
    ++lastSeenObjectIdEventCount;
    if (lastSeenObjectIdEventCount <= 2) {
      const params = messageObject['params'];
      testRunner.log('HeapProfiler.lastSeenObjectId has params: ' + !!params);
      testRunner.log('HeapProfiler.lastSeenObjectId has params.lastSeenObjectId: ' + !!params.lastSeenObjectId);
      testRunner.log('HeapProfiler.lastSeenObjectId has timestamp: ' + !!params.timestamp);
      testRunner.log('A heap stats fragment did arrive before HeapProfiler.lastSeenObjectId: ' + !!fragments.length);
      testRunner.log('');
    }
    if (lastSeenObjectIdEventCount === 2) {
      // Wait for two updates and then stop tracing. Turn off these callbacks.
      // This callback is not awaited by the caller, meaning we can re-enter
      // this callback while awaiting the below call. This would cause the count
      // to get incremented again. We avoid this by de-registering the callback.
      dp.HeapProfiler.offLastSeenObjectId(objectIdCallback);
      await dp.HeapProfiler.stopTrackingHeapObjects();
      testRunner.log('Number of heapStatsUpdate events >= number of lastSeenObjectId events: ' + (heapStatsUpdateEventCount >= lastSeenObjectIdEventCount));
      testRunner.log('At least 2 lastSeenObjectId arrived: ' + (lastSeenObjectIdEventCount >= 2));
      testRunner.log('SUCCESS: tracking stopped');
      testRunner.completeTest();
    }
  };

  dp.HeapProfiler.onLastSeenObjectId(objectIdCallback);

  dp.HeapProfiler.onHeapStatsUpdate(messageObject => {
    ++heapStatsUpdateEventCount;
    const params = messageObject['params'];
    if (heapStatsUpdateEventCount <= 2) {
      testRunner.log('HeapProfiler.heapStatsUpdate has params: ' + !!params);
    }
    const statsUpdate = params.statsUpdate;
    if (heapStatsUpdateEventCount <= 2) {
      testRunner.log('HeapProfiler.heapStatsUpdate has statsUpdate: ' + !!statsUpdate);
      testRunner.log('statsUpdate length is not zero: ' + !!statsUpdate.length);
      testRunner.log('statsUpdate length is a multiple of three: ' + !(statsUpdate.length % 3));
      testRunner.log('statsUpdate: first fragmentIndex in first update: ' + statsUpdate[0]);
      testRunner.log('statsUpdate: total count of objects is not zero: ' + !!statsUpdate[1]);
      testRunner.log('statsUpdate: total size of objects is not zero: ' + !!statsUpdate[2]);
      testRunner.log('');
    }
    fragments.push(statsUpdate);
  });

  await dp.HeapProfiler.startTrackingHeapObjects();
  testRunner.log('SUCCESS: tracking started');
})

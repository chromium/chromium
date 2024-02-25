(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Test that heap tracking actually reports data fragments.');

  await session.evaluate(`
    var junkArray = new Array(1000);
    function junkGenerator() {
      for (var i = 0; i < junkArray.length; ++i)
        junkArray[i] = '42 ' + i;
      window.junkArray = junkArray;
    }
    setInterval(junkGenerator, 0)
  `);

  var Helper = await testRunner.loadScript('resources/heap-snapshot-common.js');
  var helper = await Helper(testRunner, session);

  function arraysEqual(a, b) {
    var sa = a.join(', ');
    var sb = b.join(', ');
    if (sa === sb)
      return true;
    testRunner.log('FAIL:\na = [' + sa + ']\nb = [' + sb + ']');
    return false;
  }

  var sizes = [];
  var lastAssignedIds = [];
  dp.HeapProfiler.onLastSeenObjectId(async messageObject => {
    lastAssignedIds.push(messageObject['params']['lastSeenObjectId']);
    if (lastAssignedIds.length === 2) {
      // Wait for two updates and then stop tracing.
      var snapshot = await helper.stopRecordingHeapTimeline();
      var samples = snapshot.getSamples();
      testRunner.log('Last assigned id arrays match: ' + arraysEqual(lastAssignedIds, samples.lastAssignedIds));
      var sizesMatch = (sizes.length <= samples.sizes.length);
      testRunner.log('Size arrays length is correct: ' + sizesMatch);
      if (!sizesMatch) {
        // Print mismatch:
        arraysEqual(sizes, samples.sizes);
      }
      var sizesNonGrowing = true;
      for (var i = 0; i < samples.sizes.length; i++) {
        if ((sizes[i] === undefined && samples.sizes[i] !== 0) || (sizes[i] < samples.sizes[i])) {
          sizesNonGrowing = false;
          testRunner.log('FAIL: total size of live objects from interval cannot increase.');
          // Print mismatch:
          arraysEqual(sizes, samples.sizes);
        }
      }
      testRunner.log('Sizes non growing: ' + sizesNonGrowing);
      testRunner.completeTest();
    }
  });

  dp.HeapProfiler.onHeapStatsUpdate(messageObject => {
    var samples = messageObject['params']['statsUpdate'];
    for (var i = 0; i < samples.length; i += 3) {
      var index = samples[i];
      sizes[index] = samples[i+2];
    }
  });

  await dp.HeapProfiler.startTrackingHeapObjects();
  testRunner.log('Tracking started');
})

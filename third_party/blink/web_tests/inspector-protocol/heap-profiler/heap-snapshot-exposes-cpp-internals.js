(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
      'Tests whether a heap snapshot contains any "InternalNode" or "blink::" objects. This test is useful to check that cppgc_enable_object_names gn arg is enabled.');

    await dp.Profiler.enable();

    // Take a heap snapshot.
    let snapshot_string = '';
    function onChunk(message) {
      snapshot_string += message.params.chunk;
    }
    dp.HeapProfiler.onAddHeapSnapshotChunk(onChunk)
    await dp.HeapProfiler.takeHeapSnapshot({ reportProgress: false, exposeInternals: true });
    const s = JSON.parse(snapshot_string);

    // Iterate all nodes and check the name against "InternalNode" and Blink C++ namespace "blink::".
    const nameIndex = s.snapshot.meta.node_fields.indexOf('name');
    const nodeLength = s.snapshot.meta.node_fields.length;
    let foundInternalNode = false;
    let foundBlinkNameSpace = false;
    for (let i = 0; i < (nodeLength * s.snapshot.node_count); i += nodeLength) {
      let nodeName = s.strings[s.nodes[i + nameIndex]];
      if (nodeName === 'InternalNode') {
        foundInternalNode = true;
      } else if (nodeName.startsWith('blink::')) {
        foundBlinkNameSpace = true;
      }
    }

    if (foundInternalNode) {
      testRunner.log('InternalNode: Found');
    } else {
      testRunner.log('InternalNode: Not found');
    }
    if (foundBlinkNameSpace) {
      testRunner.log('Blink namespace: Found');
    } else {
      testRunner.log('Blink namespace: Not found');
    }

    testRunner.completeTest();
  })

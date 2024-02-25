(async function(/** @type {import('test_runner').TestRunner} */ testRunner, session) {
  self['Common'] = {};
  self['TextUtils'] = {};
  self['HeapSnapshotModel'] = {};
  self['HeapSnapshotWorker'] = {};

  // This script is supposed to be evaluated in inspector-protocol/heap-profiler tests
  // and the relative paths below are relative to that location.
  await testRunner.loadScript('./resources/HeapSnapshotLoader.js');

  // Expose the (de)serialize code from Common because the worker expects it on self.
  // TODO(https://crbug.com/680046) Remove the dupe code below.
  self.serializeUIString = Common.serializeUIString;
  self.deserializeUIString = Common.deserializeUIString;

  async function takeHeapSnapshotInternal(command) {
    var loader = new HeapSnapshotWorker.HeapSnapshotLoader();
    function onChunk(messageObject) {
      loader.write(messageObject['params']['chunk']);
    }
    session.protocol.HeapProfiler.onAddHeapSnapshotChunk(onChunk);
    await command();
    session.protocol.HeapProfiler.offAddHeapSnapshotChunk(onChunk);
    testRunner.log('Took heap snapshot');
    loader.close();
    await new Promise(r => setTimeout(r));
    var snapshot = loader.buildSnapshot(false);
    testRunner.log('Parsed snapshot');
    return snapshot;
  }

  function firstRetainingPath(node) {
    for (var iter = node.retainers(); iter.hasNext(); iter.next()) {
      var retainingEdge = iter.retainer;
      var retainer = retainingEdge.node();
      if (retainingEdge.isWeak() ||
          retainer.distance() >= node.distance()) continue;
      var path = firstRetainingPath(retainer);
      path.unshift(retainer);
      return path;
    }
    return [];
  }

  return {
    firstRetainingPath: firstRetainingPath,

    takeHeapSnapshot: function() {
      return takeHeapSnapshotInternal(() => session.protocol.HeapProfiler.takeHeapSnapshot());
    },

    stopRecordingHeapTimeline: function() {
      return takeHeapSnapshotInternal(() => session.protocol.HeapProfiler.stopTrackingHeapObjects());
    }
  };
})

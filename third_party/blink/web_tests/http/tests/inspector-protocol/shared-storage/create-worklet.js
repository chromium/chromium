(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests sharedStorage.createWorklet event notification.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';
  const resources = baseOrigin + 'inspector-protocol/shared-storage/resources/'

  const events = [];
  let totalEventsSoFar = 0;

  async function getPromiseForEventCount(numEvents) {
    totalEventsSoFar += numEvents;
    return dp.Storage.onceSharedStorageAccessed(messageObject => {
      events.push(messageObject.params);
      return (events.length === totalEventsSoFar);
    });
  }

  await dp.Storage.setSharedStorageTracking({enable: true});

  eventPromise = getPromiseForEventCount(4);

  // The following calls should trigger events if shared storage is enabled, as
  // tracking is now enabled.
  //
  // Generates 4 events.
  await session.evaluateAsync(`
        sharedStorage.clear();
        const script_url = "${resources}shared-storage-module.js";
        sharedStorage.createWorklet(script_url)
            .then((worklet) => worklet.run("set-operation"));
  `);

  // We want to make certain that the metadata, entries, and events are done
  // updating before we retrieve them.
  await eventPromise;

  const data =
      await dp.Storage.getSharedStorageMetadata({ownerOrigin: baseOrigin});
  testRunner.log(data.result?.metadata, 'Metadata: ', ['creationTime']);
  const entriesResult =
      await dp.Storage.getSharedStorageEntries({ownerOrigin: baseOrigin});
  testRunner.log(entriesResult.result?.entries, 'Entries:');
  testRunner.log(events, 'Events: ', [
    'accessTime', 'mainFrameId', 'urnUuid', 'workletOrdinal', 'workletTargetId',
    'serializedData'
  ]);

  // Clean up shared storage.
  await session.evaluateAsync('sharedStorage.clear()');

  testRunner.completeTest();
})

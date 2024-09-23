(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage events from sharedStorageWritable fetch.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';

  const rawWriteHeader = 'clear,set;key=a;value=b;ignore_if_present,' +
      'set;key=hello;value=world,append;key=hello;value=friend,' +
      'delete;key=a';
  const writeHeader = encodeURIComponent(rawWriteHeader);
  const imageUrl = baseOrigin +
      'inspector-protocol/resources/shared-storage-write-image.php';

  async function getSharedStorageMetadata(origin) {
    const data =
        await dp.Storage.getSharedStorageMetadata({ownerOrigin: origin});
    testRunner.log(data.result?.metadata, 'Metadata: ', ['creationTime']);
  }

  async function getSharedStorageEntries(origin) {
    const entriesResult =
        await dp.Storage.getSharedStorageEntries({ownerOrigin: baseOrigin});
    testRunner.log(entriesResult.result?.entries, 'Entries:');
  }

  function dumpSharedStorageEvents(events) {
    testRunner.log(events, 'Events: ', ['accessTime', 'mainFrameId']);
  }

  const events = [];
  let totalEventsSoFar = 0;

  async function getPromiseForEventCount(numEvents) {
    totalEventsSoFar += numEvents;
    return dp.Storage.onceSharedStorageAccessed(messageObject => {
      events.push(messageObject.params);
      return (events.length === totalEventsSoFar);
    });
  }

  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Storage.setSharedStorageTracking({enable: true});

  eventPromise = getPromiseForEventCount(5);

  // The following calls should trigger events if shared storage is enabled, as
  // tracking is now enabled.
  //
  // Generates 5 events.
  session.evaluateAsync(`
      fetch("${imageUrl}?write=${writeHeader}", {sharedStorageWritable: true});
  `);

  // We wait to ensure that metadata and entries are done updating before we
  // retrieve them.
  await eventPromise;

  await getSharedStorageMetadata(baseOrigin);
  await getSharedStorageEntries(baseOrigin);
  await dumpSharedStorageEvents(events);

  testRunner.completeTest();
})

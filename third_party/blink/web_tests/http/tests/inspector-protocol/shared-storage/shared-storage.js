(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage event tracking & fetching of entries/metadata.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  async function getSharedStorageMetadata(dp, testRunner, origin) {
    const data = await dp.Storage.getSharedStorageMetadata(
      {ownerOrigin: origin});
    testRunner.log(data.result?.metadata, 'Metadata: ', ['creationTime']);
  }

  async function getSharedStorageEntries(dp, testRunner, origin) {
    const entriesResult = await dp.Storage.getSharedStorageEntries(
      {ownerOrigin: baseOrigin});
    testRunner.log(entriesResult.result?.entries, 'Entries:');
  }

  async function getSharedStorageEvents(testRunner, events) {
    testRunner.log(events, 'Events: ', ['accessTime', 'mainFrameId']);
  }

  const events = [];
  let totalEventsSoFar = 0;

  async function getPromiseForEventCount(numEvents) {
    totalEventsSoFar += numEvents;
    return dp.Storage.onceSharedStorageAccessed(messageObject => {
      // Skip testing the content of `serializedData`, as it can contain
      // non-printable characters.
      if (messageObject.params.params.serializedData !== undefined) {
        messageObject.params.params.serializedData = '';
      }
      events.push(messageObject.params);
      return (events.length === totalEventsSoFar);
    });
  }

  await dp.Storage.setSharedStorageTracking({enable: true});

  eventPromise = getPromiseForEventCount(7);

  // The following calls should trigger events if shared storage is enabled, as
  // tracking is now enabled.
  //
  // Generates 7 events.
  await session.evaluateAsync(`
        sharedStorage.set('key0-set-from-document', 'value0');
        sharedStorage.set('key1-set-from-document', 'value1',
                          {ignoreIfPresent: true});
        sharedStorage.append('key1-set-from-document', 'value1');
        sharedStorage.set('key2-set-from-document', 'value2',
                          {ignoreIfPresent: false});
        sharedStorage.set('key2-set-from-document', 'value3',
                          {ignoreIfPresent: true});
        sharedStorage.delete('key2-set-from-document');
        const script_url = "${base}shared-storage-module.js";
        sharedStorage.worklet.addModule(script_url);
  `);

  // We wait before calling into the worklet in order to ensure that
  // events are received in the expected order.
  await eventPromise;

  eventPromise = getPromiseForEventCount(10);

  // Generates 10 events.
  await session.evaluateAsync(`
        sharedStorage.run("test-operation", {keepAlive: true});
  `);

  // We wait before calling into the worklet again in order to ensure that
  // worklet events are received in the expected order.
  await eventPromise;

  eventPromise = getPromiseForEventCount(3);

  // Generates 3 events.
  await session.evaluateAsync(`
        sharedStorage.selectURL(
          "test-url-selection-operation",
          [{url: "https://google.com/"}, {url: "https://chromium.org/"}],
          {keepAlive: true});
  `);

  // We wait before calling into the worklet again in order to ensure that
  // worklet events are received in the expected order, and we also want to
  // make certain that the metadata and entries are done updating before
  // we retrieve them.
  await eventPromise;

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.log(`Clear entries`);

  eventPromise = getPromiseForEventCount(2);

  // Generates 2 events.
  await session.evaluateAsync(`sharedStorage.run("clear-operation");`);

  // We wait before calling into the document again in order to ensure that
  // events are received in the expected order
  await eventPromise;

  eventPromise = getPromiseForEventCount(1);

  // Generates 1 event.
  await session.evaluateAsync('sharedStorage.clear()');

  // We want to make certain that the metadata, entries, and events are done
  // updating before we retrieve them.
  await eventPromise;

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);
  await getSharedStorageEvents(testRunner, events);

  testRunner.completeTest();
})

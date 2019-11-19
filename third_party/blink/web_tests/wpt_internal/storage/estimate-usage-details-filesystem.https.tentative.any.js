// META: title=StorageManager: estimate() usage details for fileSystem

promise_test(async t => {
  const estimate = await navigator.storage.estimate()
  assert_equals(typeof estimate.usageDetails, 'object');
}, 'estimate() resolves to dictionary with usageDetails member');

promise_test(async t => {
  const writeSize = 1024 * 100;
  const fileSystem = await new Promise((resolve, reject) => {
    self.webkitRequestFileSystem(self.TEMPORARY, 10 * writeSize, resolve,
                                 reject);
  });
  const fileEntry = await new Promise((resolve, reject) => {
    fileSystem.root.getFile('quotaTest', {create:true}, resolve, reject);
  });
  const fileWriter = await new Promise((resolve, reject) => {
    fileEntry.createWriter(resolve, reject);
  });
  const input = new Uint8Array(writeSize);
  const blob = new Blob([input]);
  fileWriter.write(blob);
  await new Promise((resolve, reject) => {
    fileWriter.onwrite = () => { resolve(); }
    fileWriter.onerror = (event) => { reject(event.target.error); }
  });

  const estimate = await navigator.storage.estimate();
  assert_true(estimate.usageDetails.fileSystem >= writeSize);
}, 'estimate() usage details reflects increase in fileSystem after write ' +
      ' operation');

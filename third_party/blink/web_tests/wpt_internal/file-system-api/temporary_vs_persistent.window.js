// META: script=resources/fs_helpers.js

for (let kind of ['TEMPORARY', 'PERSISTENT']) {
  promise_test(async testCase => {
      const first = await getFileSystem(self[kind]);
      const second = await getFileSystem(self[kind]);
      assert_equals(first.name, second.name);
  }, `requestfileSystem returns the same ${kind} filesystem`);
}

promise_test(async testCase => {
  const temporaryFs = await getFileSystem(self.TEMPORARY);
  const persistentFs = await getFileSystem(self.PERSISTENT);

  assert_not_equals(temporaryFs.root, persistentFs.root);
}, 'TEMPORARY and PERSISTENT file systems have different names');

promise_test(async testCase => {
  await requestStorageQuota(navigator.webkitPersistentStorage, kDesiredQuota);
  const temporaryFs = await getFileSystem(self.TEMPORARY);
  const persistentFs = await getFileSystem(self.PERSISTENT);

  const path = '/hello.txt';
  const temporaryData = 'Hello temporary world!';
  const persistentData = 'Hello persistent world!';

  await writeFile(temporaryFs, path, temporaryData);
  await writeFile(persistentFs, path, persistentData);

  const temporaryFileEntry = await getFileSystemFileEntry(temporaryFs, path);
  const persistentFileEntry = await getFileSystemFileEntry(persistentFs, path);

  assert_equals(temporaryFileEntry.name, persistentFileEntry.name);
  assert_equals(temporaryFileEntry.fullPath, persistentFileEntry.fullPath);

  assert_not_equals(temporaryFileEntry.filesystem.name,
                    persistentFileEntry.filesystem.name);
  assert_equals(temporaryFileEntry.filesystem, temporaryFs);
  assert_equals(persistentFileEntry.filesystem, persistentFs);

  assert_equals(await readFile(temporaryFs, path). temporaryData);
  assert_equals(await readFile(persistentFs, path). persistentData);
}, 'TEMPORARY and PERSISTENT roots point to different directories');
// Quota requested for the file system tests.
const kDesiredQuota = 1024 * 1024;

// Wrapper around DeprecatedStorageQuota.requestQuota().
async function requestStorageQuota(storage, newQuotaInBytes) {
  return new Promise((resolve, reject) => {
    storage.requestQuota(newQuotaInBytes, resolve, reject);
  });
}

// Promise wrapper for self.webkitRequestFileSystem().
async function getFileSystem(kind = self.TEMPORARY) {
  return new Promise((resolve, reject) => {
    self.webkitRequestFileSystem(kind, kDesiredQuota, resolve, reject);
  });
}

// Promise wrapper for FileSystem.getFile().
async function getFileSystemFileEntry(fileSystem, path, options = {}) {
  return new Promise((resolve, reject) => {
    fileSystem.root.getFile(path, options, resolve, reject);
  });
}

// Promise wrapper for FileSystemFileEntry.createWriter().
async function createFileSystemFileEntryWriter(fileEntry) {
  return new Promise((resolve, reject) => {
    fileEntry.createWriter(resolve, reject);
  });
}

// Promise wrapper for one FileWriter.write() call.
async function writeFileWriterData(fileWriter, data) {
  const blob = new Blob([data], { type: 'application/octet-stream '});
  return new Promise((resolve, reject) => {
    fileWriter.onwriteend = () => { resolve(); };
    fileWriter.onerror = event => { reject(event.target.error); };
    fileWriter.write(blob);
  });
}

// Promise wrapper for FileSystemFileEntry.file().
async function getFileSystemFileEntryFile(fileEntry) {
  return new Promise((resolve, reject) => {
    fileEntry.file(resolve, reject);
  });
}

// Promise-based helper for writing a file via the FileSystem API.
async function writeFile(fileSystem, path, data) {
  const fileEntry = await getFileSystemFileEntry(fileSystem, path,
                                                 { create: true });
  const fileWriter = await createFileSystemFileEntryWriter(fileEntry);
  await writeFileWriterData(fileWriter, data);
}

// Promise-based helper for reading a file via the FileSystem API.
async function readFile(fileSystem, path) {
  const fileEntry = await getFileSystemFileEntry(fileSystem, path);
  const file = await getFileSystemFileEntryFile(fileEntry);
  return await file.text();
}

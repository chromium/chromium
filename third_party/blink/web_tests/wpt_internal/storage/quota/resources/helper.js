const requestQuota = (size) => {
  return new Promise((resolve, reject) => {
    window.webkitStorageInfo.requestQuota(PERSISTENT, size, resolve, reject);
  });
};

const usageDetails = async (type) => {
  return new Promise((resolve, reject) => {
    window.webkitStorageInfo.queryUsageAndQuota(
      type,
      (usage, quota) => resolve({ usage: usage, quota: quota }),
      reject
    );
  })
    .then((details) => details)
    .catch((error) => {
      throw error;
    });
};

const requestFileSystemAndWriteDummyFile = async (type) => {
  return new Promise((resolve, reject) =>
    webkitRequestFileSystem(type, 512, resolve, reject)
  )
    .then(
      (filesystem) =>
        new Promise((resolve, reject) =>
          filesystem.root.getFile(
            "dummy-file",
            { create: true },
            resolve,
            reject
          )
        )
    )
    .then(
      (entry) =>
        new Promise((resolve, reject) => entry.createWriter(resolve, reject))
    )
    .then((writer) => {
      return new Promise((resolve, reject) => {
        writer.onwriteend = resolve;
        writer.error = reject;
        writer.write(new Blob(["Dummy text for some file weight..."]));
      });
    });
};

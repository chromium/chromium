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

// META: script=resources/fs_helpers.js

promise_test(async t => {
    const fs = await getFileSystem(self.TEMPORARY);
    const path = '/test-path.txt';
    const contents = 'Hello World!';
    await writeFile(fs, path, contents);
    const entry = await getFileSystemFileEntry(fs, path);
    const url = entry.toURL();

    // Use XHR rather than fetch to load the url, since Chrome's fetch
    // implementation doesn't support loading filesystem: URLs.
    const response = await new Promise((resolve, reject) => {
        const req = new XMLHttpRequest();
        req.onload = t.step_func(e => {
            resolve(req.responseText);
        });
        req.onerror = reject;
        req.onabort = reject;
        req.open('GET', url);
        req.send();
    });
    assert_equals(response, contents);
}, 'Loading a filesystem: url works.');

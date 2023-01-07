// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const child_process = require('child_process');
const path = require('path');

// Returns whether 'gsutil' is available or not.
function exists() {
  try {
    child_process.execSync('gsutil', {stdio: 'ignore'});
    return true;
  } catch (ex) {
    return false;
  }
}

// Downloads a list of files.
// `map` is a map of {<gs:// url> : <local-filename>}.
// `output` is the local directory to download the files to.
async function downloadFiles(map, output) {
  let downloaded = 0;
  const total = Object.keys(map).length;
  process.stdout.write(`\r*** ${downloaded}/${total} files downloaded.`);
  function exec(cmd, options) {
    return new Promise((resolve, reject) => {
      child_process.exec(cmd, options, () => {
        ++downloaded;
        process.stdout.write(`\r*** ${downloaded}/${total} files downloaded.`);
        resolve();
      });
    });
  }

  const promises = [];
  for (const url in map) {
    const cmds = [
      'gsutil',
      'cp',
      url,
      path.join(output, map[url]),
    ];
    promises.push(exec(cmds.join(' '), {stdio: 'ignore'}));
  }

  await Promise.all(promises);
}

module.exports = {
  exists,
  downloadFiles,
};

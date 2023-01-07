// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const child_process = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

// QueryRunner uses the swarming binary to retrieve data from the swarming
// servers.
class QueryRunner {
  constructor(server) {
    this.swarming_server_ = server;
    this.bin_ = './tools/luci-go/swarming';
  }

  // Retrieves the list of tasks matching `tag`.
  retrieveTasks(tag, fields = [
    'task_id', 'run_id', 'failure', 'state', 'tags'
  ]) {
    const cmds = [
      this.bin_, 'tasks', '-S', this.swarming_server_,
      `-field="items(${fields.join(',')})"`, `-tag="${tag}"`
    ];
    const output = child_process.execSync(cmds.join(' '));
    return JSON.parse(output.toString());
  }

  // Returns a promoise for a parsed JSON object from the contents in `filename`
  // for the specified `task_id`.
  retrieveJSONFile(task_id, filename) {
    // This is going to create some temp directory. So put this in a try/finally
    // block so that the temp directory gets cleaned up correctly.
    let temp_dir = undefined;
    try {
      temp_dir = fs.mkdtempSync(path.join(os.tmpdir(), 'cr-fetch'));
      return this.retrieveFile_(task_id, filename, temp_dir);
    } catch (e) {
      console.error(e);
    } finally {
      if (temp_dir) {
        try {
          fs.rmdirSync(temp_dir, {recursive: true, force: false});
        } catch (e) {
          console.error(`Failed to cleanup temp directory at ${temp_dir}.`);
          console.error(e);
        }
      }
    }
  }

  retrieveFile_(task_id, filename, temp_dir) {
    return new Promise((resolve, reject) => {
      const cmds = [
        this.bin_, 'collect', '-S', this.swarming_server_,
        `-output-dir="${temp_dir}"`, task_id
      ];
      const output = child_process.exec(cmds.join(' '), () => {
        const filepath = path.join(temp_dir, task_id, filename);
        resolve(JSON.parse(fs.readFileSync(filepath).toString()));
      });
    });
  }
};

module.exports = {
  QueryRunner,
};

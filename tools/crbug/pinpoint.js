// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const process = require('child_process');

class PinpointJob {
  constructor(json) {
    this.user_ = json.created_by;
    this.timestamp_ = new Date(json.create_time.seconds * 1000);
    this.crbug_ = json.job_spec.monorail_issue;
    this.url_ = undefined;
    this.project_ = undefined;

    if (this.crbug_) {
      const project = this.crbug_.project;
      const num = this.crbug_.issue_id;
      this.url_ =
          `https://bugs.chromium.org/p/${project}/issues/detail?id=${num}`;
      this.project_ = project;
    }
  }

  get url() {
    return this.url_;
  }
  get timestamp() {
    return this.timestamp_;
  }
  get project() {
    return this.project_;
  }
};

class Pinpoint {
  constructor() {}

  listJobs(useremail) {
    const args =
        ['pinpoint', 'list-jobs', '--json', '--filter', `user=${useremail}`];
    for (let tries = 0; tries < 3; ++tries) {
      try {
        const stdout = process.execSync(args.join(' ')).toString().trim();
        const json = JSON.parse(stdout);
        if (json) {
          const jobs = json.map(j => new PinpointJob(j));
          return jobs;
        }
      } catch (ex) {
      }
    }
    return [];
  }
};

async function test() {
  const pinpoint = new Pinpoint();
  const jobs = pinpoint.listJobs('sadrul@google.com');
  console.log(jobs.filter(j => j.url)
                  .map(j => `${j.timestamp.toLocaleDateString()},${j.url}`));
}

module.exports = {
  Pinpoint,
};

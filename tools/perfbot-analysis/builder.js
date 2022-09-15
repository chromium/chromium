// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const runner = require('./query-runner.js');
const swarming = require('./swarming-task.js');

// Build represents a specific run of a bot, i.e. it is associated with a
// [platform, build-number].
class Build {
  static PRODUCT_SWARMING_SERVER = {
    'chromium': 'chromium-swarm.appspot.com',
    'chrome': 'chrome-swarming.appspot.com',
  };
  static PRODUCT_BUILD_ADDRESS = {
    'chromium': 'luci.chromium.ci',
    'chrome': 'luci.chrome.ci',
  };

  constructor(url) {
    this.product_ = undefined;
    this.platform_ = undefined;
    this.swarming_server_ = undefined;
    this.build_address_ = undefined;
    this.build_number_ = undefined;
    this.extractInfoFromUrl_(url);
    this.query_runner_ = new runner.QueryRunner(this.swarming_server_);
  }

  get platform() {
    return this.platform_;
  }

  findSwarmingTask() {
    const build_address =
        [this.build_address_, this.platform_, this.build_number_].join('/');
    const results =
        this.query_runner_.retrieveTasks(`build_address:${build_address}`);
    const task =
        new swarming.ParentSwarmingTask(this.query_runner_, results[0]);
    return task;
  }

  extractInfoFromUrl_(url) {
    const info = {};
    if (typeof (url) === 'string') {
      url = new URL(url);
      const parts = url.pathname.split('/').map(x => decodeURI(x));
      info.product = parts[3];
      info.platform = parts[6];
      info.buildnumber = parts[7];
    } else {
      info.product = url.product;
      info.platform = url.platform;
      info.build_number = url.build_number;
    }
    this.product_ = info.product;
    this.platform_ = info.platform;
    this.swarming_server_ = Build.PRODUCT_SWARMING_SERVER[info.product];
    this.build_address_ = Build.PRODUCT_BUILD_ADDRESS[info.product];
    this.build_number_ = info.buildnumber;
  }
};

module.exports = {
  Build,
};

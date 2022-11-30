// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// SwarmingTask represents a task in the swarming server.
class SwarmingTask {
  constructor(runner, json) {
    this.query_runner_ = runner;
    this.task_id_ = json.task_id;
    this.run_id_ = json.run_id || undefined;
    this.failure_ = json.failure;
    this.state_ = json.state;
    this.tags_ = {};
    json.tags.map(tag => {
      const parts = tag.split(':');
      this.tags_[parts.shift()] = parts.join(':');
    });
  }

  get name() {
    return this.tags_.name;
  }
  get task_id() {
    return this.task_id_;
  }
};

// A ChildSwarmingTask is the task that actually runs tests and contains
// artifacts. So to find all the failed tests, it downloads the artifacts from
// the swarming server, and parses the `output.json` file.
class ChildSwarmingTask extends SwarmingTask {
  // Returns a promoise for a parsed JSON object from the contents in
  // `filename` for this task.
  async downloadJSONFile(filename) {
    return this.query_runner_.retrieveJSONFile(this.task_id_, filename);
  }
};

// A ParentSwarmingTask itself does not run tests. It delegates the job of
// running the tests to a ChildSwarmingTask. Therefore, to find all the all
// failing tests, it needs to aggregate the failed tests of all its children
// swarming tasks.
class ParentSwarmingTask extends SwarmingTask {
  findChildTasks() {
    const results =
        this.query_runner_.retrieveTasks(`parent_task_id:${this.run_id_}`);
    return results.map(r => new ChildSwarmingTask(this.query_runner_, r));
  }
};

module.exports = {
  ParentSwarmingTask,
};

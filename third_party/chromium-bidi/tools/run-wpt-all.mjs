#!/usr/bin/env node

/**
 * Copyright 2024 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {spawnSync} from 'child_process';
import {join} from 'path';

import {packageDirectorySync} from 'pkg-dir';

function getNpmArgs() {
  const args = ['run', 'wpt'];

  const file = process.argv.slice(2)[0];

  if (file) {
    args.push('--');
    args.push(file);
  }

  return args;
}

function runWpt(options) {
  console.log(`Running WPT with options:\n${JSON.stringify(options, null, 2)}`);

  const cwd = packageDirectorySync();
  spawnSync('npm', getNpmArgs(), {
    stdio: 'inherit',
    shell: true,
    env: {
      UPDATE_EXPECTATIONS: 'true',
      LOG_FILE: `${join(cwd, 'logs')}/run-all.log`,
      ...process.env,
      ...options,
    },
    cwd,
    ...options,
  });
}

runWpt({
  CHROMEDRIVER: true,
  HEADLESS: false,
});
runWpt({
  CHROMEDRIVER: true,
  HEADLESS: true,
});
runWpt({
  HEADLESS: true,
});

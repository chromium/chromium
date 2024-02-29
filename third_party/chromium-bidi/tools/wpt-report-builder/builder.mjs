/**
 * Copyright 2023 Google LLC.
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
import child_process from 'child_process';
import fs from 'fs';

import {generateReport} from './formatter.mjs';

function getCurrentCommit() {
  if (process.env.GITHUB_SHA) {
    // If triggered by GitHub Actions, use the commit SHA provided by GitHub.
    return process.env.GITHUB_SHA;
  }
  return child_process.execSync('git rev-parse HEAD').toString().trim();
}

function readReport(filePath) {
  const rawReport = fs.readFileSync(filePath);
  return JSON.parse(rawReport);
}

function getReportPath() {
  return process.argv.slice(2)[0];
}

function getOutputPath() {
  return process.argv.slice(2)[1];
}

fs.writeFileSync(
  getOutputPath(),
  generateReport(readReport(getReportPath()), getCurrentCommit())
);

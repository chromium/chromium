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

import fs from 'fs';

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

function generateHtml(report) {
  const stat = {all: 0, pass: 0};
  for (const test of report.results) {
    if (test.status !== 'OK' && test.subtests.length === 0) {
      stat.all++;
    } else {
      for (const sub of test.subtests) {
        stat.all++;
        if (sub.status === 'PASS') {
          stat.pass++;
        }
      }
    }
  }

  return `
    <!DOCTYPE html>
    <html lang="en">
    <meta charset="utf-8">
    <title>BiDi-CDP Mapper</title>
    <style>
      body { font-family: Roboto, serif; font-size: 13px; color: #202124; }
      .path { font-family: Menlo, Consolas, Monaco, Liberation Mono, Lucida Console, monospace; line-height: 180%; padding: 5px 18px; margin: 0; }
      .top { box-shadow: 0 1px 4px rgba(0, 0, 0, 0.15), 0 1px 6px rgba(0, 0, 0, 0.2); border-radius: 8px; margin: auto; padding: 60px; max-width: 1200px; }
      .test-card { padding-left: 20px; max-width: 1200px; }
      .divider { margin-left: 20px; height: 1px; background: #a0a0a0; }
      .non-collapsible-item { padding-left: 27px; }
      .stat { float: right }
      .pass { background: #D5F2D7; }
      .part { background: #F2EDD5; }
      .fail { background: #F2D7D5; }
    </style>
    <div class="top">
      <h1>WPT test results</h1>
      <h2>
        ${stat.pass} / ${stat.all}
      </h2>
      <div>
        ${report.results.map(generateTestReport).join('')}
      </div>
    </div>`;
}

function generateTestReport(test) {
  const stat = {all: 0, pass: 0};
  if (test.status !== 'OK' && test.subtests.length === 0) {
    stat.all++;
  } else {
    for (const sub of test.subtests) {
      stat.all++;
      if (sub.status === 'PASS') {
        stat.pass++;
      }
    }
  }

  return `
    <div class="divider"></div>
    <div class="test-card">
      <details>
        <summary class="path ${
          stat.all === stat.pass ? 'pass' : stat.pass === 0 ? 'fail' : 'part'
        }">
          ${test.test}
          <span class="stat" ><b>${stat.pass}/${stat.all}</b></span>
        </summary>
        ${test.subtests.map(generateSubtestReport).join('')}
      </details>
    </div>`;
}

function generateSubtestReport(subtest) {
  return `
      <div class="divider"></div>
      <div class="test-card">
        <p class="non-collapsible-item path ${
          subtest.status === 'PASS' ? 'pass' : 'fail'
        }">
          ${subtest.name}
          <span class="stat"><b>${subtest.status}</b></span>
        </p>
      </div>
`;
}

const result = generateHtml(readReport(getReportPath()));

fs.writeFileSync(getOutputPath(), result);

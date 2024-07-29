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
import path from 'path';
import url from 'url';

import yargs from 'yargs';
import {hideBin} from 'yargs/helpers';

import {apply2023Filter} from './filter-2023.mjs';
import {generateReport} from './formatter.mjs';

const __dirname = url.fileURLToPath(new URL('.', import.meta.url));

export function parseCommandLineArgs() {
  return yargs(hideBin(process.argv))
    .option('bidi', {
      describe: 'A list of BiDi WPT test reports to parse',
      type: 'array',
      demandOption: true,
    })
    .option('interop', {
      describe: 'A list of interop WPT test reports to parse',
      type: 'array',
      demandOption: true,
    })
    .option('out', {
      describe: 'Output file for the generated HTML',
      type: 'string',
      demandOption: true,
    })
    .option('out-label-2023', {
      describe: 'Output file for the generated HTML filtered by label',
      type: 'string',
      demandOption: true,
    })
    .exitProcess(true)
    .parseSync();
}

function getCurrentCommit() {
  if (process.env.GITHUB_SHA) {
    // If triggered by GitHub Actions, use the commit SHA provided by GitHub.
    return process.env.GITHUB_SHA;
  }
  return child_process.execSync('git rev-parse HEAD').toString().trim();
}

function getWptCommit() {
  const output = child_process
    .execSync('git submodule status')
    .toString()
    .trim();

  const wptStatus = output
    .split('\n')
    .filter((line) => line.includes('wpt'))
    .at(0);

  return wptStatus.split('wpt').at(0).trim().replace('-', '').replace('+', '');
}

function getChromeVersion() {
  const version = fs.readFileSync(
    path.join(__dirname, '../../.browser'),
    'utf8'
  );

  return version.split('@')[1];
}

/**
 *
 * @param {string[]} filePaths
 */
function readReport(filePaths) {
  const results = [];
  for (const filePath of filePaths) {
    if (!fs.existsSync(filePath)) {
      continue;
    }
    const json = fs.readFileSync(filePath, 'utf8');
    // File may be empty if the interop test shard failed
    if (!json) {
      continue;
    }
    const parsedReport = JSON.parse(json);
    results.push(...parsedReport.results);
  }

  if (!results.length) {
    return undefined;
  }

  return {
    results,
  };
}
const args = parseCommandLineArgs();
const bidiReports = args.bidi;
const interopReports = args.interop;
const outputPath = args.out;
const filteredOutputPath = args.outLabel2023;

const reportData = readReport(bidiReports);
const reportInteropData = readReport(interopReports);
const filteredReportData = apply2023Filter(reportData);
const filteredInteropReportData = apply2023Filter(reportInteropData);

if (filteredReportData === undefined) {
  throw Error('filteredReportData is undefined');
}

const currentCommit = getCurrentCommit();
const chromeVersion = getChromeVersion();
const wptCommit = getWptCommit();

fs.writeFileSync(
  outputPath,
  generateReport(
    reportData,
    reportInteropData,
    currentCommit,
    chromeVersion,
    wptCommit
  )
);

fs.writeFileSync(
  filteredOutputPath,
  generateReport(
    filteredReportData,
    filteredInteropReportData,
    currentCommit,
    chromeVersion,
    wptCommit
  )
);

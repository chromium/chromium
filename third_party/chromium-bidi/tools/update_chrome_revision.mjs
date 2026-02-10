#!/usr/bin/env node

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

import {execSync} from 'child_process';
import {writeFile, readFile} from 'fs/promises';

import * as actions from '@actions/core';
import {SemVer} from 'semver';

import packageJson from '../package.json' with {type: 'json'};

async function getVersionAndRevisionForCanary() {
  const result = await fetch(
    'https://googlechromelabs.github.io/chrome-for-testing/last-known-good-versions.json',
  ).then((response) => {
    return response.json();
  });

  const {version, revision} = result.channels['Canary'];

  return {
    version,
    revision,
  };
}

async function updateDevToolsProtocolVersion(revision) {
  const currentProtocol = packageJson.devDependencies['devtools-protocol'];
  const command = `npm view "devtools-protocol@<=0.0.${revision}" version | tail -1`;

  const bestNewProtocol = execSync(command, {
    encoding: 'utf8',
  })
    .split(' ')[1]
    .replace(/'|\n/g, '');

  const buffer = await readFile('./package.json');
  const update = buffer
    .toString()
    .replace(
      `"devtools-protocol": "${currentProtocol}"`,
      `"devtools-protocol": "${bestNewProtocol}"`,
    );
  await writeFile('./package.json', update);
}

const currentVersion = (await readFile('./.browser', 'utf8')).split('@').pop();
console.log(`Current pinned version is: ${currentVersion}`);

const {version, revision} = await getVersionAndRevisionForCanary();

const oldSemVer = new SemVer(currentVersion, true);
const newSemVer = new SemVer(version, true);

let message = `update the pinned browser version to ${version}`;

if (newSemVer.compare(oldSemVer) <= 0) {
  // Exit the process without setting up version
  console.warn(
    `Version ${version} is older or the same as the current ${currentVersion}`,
  );
  process.exit(0);
} else if (newSemVer.major === oldSemVer.major) {
  message = `build(chrome): ${message}`;
} else {
  message = `feat(chrome)!: ${message}`;
}

actions.setOutput('commit', message);

console.log(`Chrome Canary version is: ${version} (${revision})`);
await updateDevToolsProtocolVersion(revision);

const browserVersion = `chrome@${version}`;
await writeFile('./.browser', browserVersion, 'utf-8');

// Create new `package-lock.json` as we update devtools-protocol
execSync('npm install --ignore-scripts');
// Make sure that the `package-lock.json` is formatted correctly
execSync(`npx prettier --write ./package.json`);

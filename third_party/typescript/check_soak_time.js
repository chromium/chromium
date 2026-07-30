// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script to check for NPM package versions that have soaked for
 * at least 3 weeks. Need to separately install 'npm' for the script to run
 * successfully.
 *
 * This is intended to be run by developers before updating versions in
 * 3pp/3pp.pb.
 */

import assert from 'node:assert';
import {execSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function main() {
  const pbFile = path.join(__dirname, 'linux-amd64', '3pp', '3pp.pb');

  assert.ok(fs.existsSync(pbFile), `Error: Could not find ${pbFile}`);

  const content = fs.readFileSync(pbFile, 'utf-8');

  // Extract package name and version from the first url block
  const urlBlockRegex = /url\s*\{([^{}]*)\}/;
  const match = urlBlockRegex.exec(content);
  assert.ok(!!match, 'Could not find url block in 3pp.pb');

  const blockContent = match[1];
  const urlMatch = blockContent.match(/download_url:\s*"(.*?)"/);
  const versionMatch = blockContent.match(/version:\s*"(.*?)"/);
  assert.ok(!!urlMatch, 'Could not find download_url in url block');
  assert.ok(!!versionMatch, 'Could not find version in url block');

  const url = urlMatch[1];
  const installedVersion = versionMatch[1];
  assert.ok(
      !!installedVersion, 'Could not parse installed version from url block');
  const pkgMatch = url.match(
      /registry\.npmjs\.org\/(?<packageName>(?:@[^\/]+\/)?[^\/]+)\/-\//);
  assert.ok(!!pkgMatch, 'Could not parse package name from url block');
  const pkg = pkgMatch.groups['packageName'];

  const now = new Date();
  // 3 weeks in milliseconds
  const soakMs = 3 * 7 * 24 * 60 * 60 * 1000;
  const targetTime = new Date(now.getTime() - soakMs);

  console.info(`Current time: ${now.toUTCString()}`);
  console.info(`Target soak time (3 weeks ago): ${targetTime.toUTCString()}\n`);

  const URL_TEMPLATE =
      'https://registry.npmjs.org/{packageName}/-/{subPackageName}-{version}.tgz';

  console.info(`Checking ${pkg}...`);
  console.info(`  Installed version: ${installedVersion}\n`);

  let timesData = null;

  try {
    const stdout = execSync(`npm view ${pkg} time --json`, {encoding: 'utf-8'});
    timesData = JSON.parse(stdout);
  } catch (error) {
    console.error(`Error for ${pkg}: ${error.message}`, error.stderr);
    process.exit(1);
  }

  const installedTimestampStr = timesData[installedVersion];
  assert.ok(
      !!installedTimestampStr,
      `Could not find timestamp for installed version ${installedVersion}`);
  const installedTimestamp = new Date(installedTimestampStr);

  const validVersions = [];
  for (const [version, timestampStr] of Object.entries(timesData)) {
    if (version === 'modified' || version === 'created') {
      continue;
    }

    const timestamp = new Date(timestampStr);

    if (timestamp <= targetTime && timestamp > installedTimestamp) {
      validVersions.push({version, timestamp});
    }
  }

  if (validVersions.length === 0) {
    console.info('  No versions found that have soaked for at least 3 weeks.');
    return;
  }

  validVersions.sort((a, b) => a.timestamp - b.timestamp);
  validVersions.forEach(latest => {
    const url = URL_TEMPLATE.replaceAll('{packageName}', pkg)
                    .replaceAll('{subPackageName}', pkg.split('/')[1])
                    .replaceAll('{version}', latest.version);
    console.info(`  Eligible version: ${latest.version}`);
    console.info(`  Eligible URL:     ${url}`);
    console.info(
        `  Published:           ${latest.timestamp.toUTCString()}\n`);
  });
}

main();

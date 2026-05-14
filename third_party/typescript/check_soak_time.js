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
  const pbFile = path.join(__dirname, '3pp', '3pp.pb');

  assert.ok(fs.existsSync(pbFile, `Error: Could not find ${pbFile}`));

  const content = fs.readFileSync(pbFile, 'utf-8');

  // Extract package names from download_urls
  const urlRegex = /download_url:\s*"(.*?)"/g;
  const packages = new Set();
  let match = null;
  while ((match = urlRegex.exec(content)) !== null) {
    const url = match[1];
    const pkgMatch = url.match(
        /registry\.npmjs\.org\/(?<packageName>(?:@[^\/]+\/)?[^\/]+)\/-\//);
    assert.ok(!!pkgMatch);
    packages.add(pkgMatch.groups['packageName']);
  }

  assert.ok(packages.size > 0, 'No packages found in 3pp.pb');

  const now = new Date();
  // 3 weeks in milliseconds
  const soakMs = 3 * 7 * 24 * 60 * 60 * 1000;
  const targetTime = new Date(now.getTime() - soakMs);

  console.info(`Current time: ${now.toUTCString()}`);
  console.info(`Target soak time (3 weeks ago): ${targetTime.toUTCString()}\n`);

  const URL_TEMPLATE =
      'https://registry.npmjs.org/{packageName}/-/{subPackageName}-{version}.tgz';

  const sortedPackages = Array.from(packages).sort();
  for (const pkg of sortedPackages) {
    console.info(`Checking ${pkg}...`);

    let timesData = null;

    try {
      const stdout =
          execSync(`npm view ${pkg} time --json`, {encoding: 'utf-8'});
      timesData = JSON.parse(stdout);
    } catch (error) {
      console.error(`Error for ${pkg}: ${error.message}`, error.stderr);
      process.exit(1);
    }

    const validVersions = [];
    for (const [version, timestampStr] of Object.entries(timesData)) {
      if (version === 'modified' || version === 'created') {
        continue;
      }

      const timestamp = new Date(timestampStr);
      assert.ok(!Number.isNaN(timestamp.getTime()));

      if (timestamp <= targetTime) {
        validVersions.push({version, timestamp});
      }
    }

    if (validVersions.length > 0) {
      validVersions.sort((a, b) => a.timestamp - b.timestamp);
      const latest = validVersions.at(-1);
      const url = URL_TEMPLATE.replaceAll('{packageName}', pkg)
                      .replaceAll('{subPackageName}', pkg.split('/')[1])
                      .replaceAll('{version}', latest.version);
      console.info(`  Recommended version: ${latest.version}`);
      console.info(`  Recommended URL:     ${url}`);
      console.info(
          `  Published:           ${latest.timestamp.toUTCString()}\n`);
      continue;
    }

    console.info('  No versions found that have soaked for at least 3 weeks.');
  }
}

main();

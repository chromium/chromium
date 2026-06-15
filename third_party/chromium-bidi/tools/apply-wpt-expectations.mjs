#!/usr/bin/env node

/**
 * Copyright 2026 Google LLC.
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

import {execFileSync} from 'child_process';
import fs from 'fs';
import {join} from 'path';

// Change working directory to repo root
process.chdir(join(import.meta.dirname, '..'));

const tempDir = join(process.cwd(), 'out', 'tmp-wpt-artifacts');

function cleanTempDir() {
  if (fs.existsSync(tempDir)) {
    fs.rmSync(tempDir, {recursive: true, force: true});
  }
}

try {
  let runId = process.argv[2];
  if (runId) {
    console.log(`Using user-specified run ID: ${runId}`);
  } else {
    // 1. Get branch name
    let branch = '';
    try {
      const prJson = execFileSync(
        'gh',
        ['pr', 'view', '--json', 'headRefName'],
        {
          encoding: 'utf-8',
          stdio: ['ignore', 'pipe', 'ignore'],
        },
      );
      branch = JSON.parse(prJson).headRefName;
    } catch {
      branch = execFileSync('git', ['branch', '--show-current'], {
        encoding: 'utf-8',
      }).trim();
    }

    if (!branch) {
      throw new Error('Could not determine current git branch');
    }
    console.log(`Current branch: ${branch}`);

    // 2. Find latest run of WPT workflow
    console.log('Finding latest WPT workflow run...');
    const runListOutput = execFileSync(
      'gh',
      [
        'run',
        'list',
        '--workflow',
        'WPT',
        '--branch',
        branch,
        '--limit',
        '1',
        '--json',
        'databaseId,status,conclusion',
      ],
      {encoding: 'utf-8'},
    );
    const runs = JSON.parse(runListOutput);
    if (runs.length === 0) {
      throw new Error(`No WPT workflow runs found for branch "${branch}"`);
    }
    const run = runs[0];
    runId = run.databaseId;
    console.log(
      `Found run ID ${runId} (Status: ${run.status}, Conclusion: ${run.conclusion})`,
    );
  }

  // 3. Get artifacts list
  const artifactsOutput = execFileSync(
    'gh',
    ['api', `repos/:owner/:repo/actions/runs/${runId}/artifacts`],
    {encoding: 'utf-8'},
  );
  const artifacts = JSON.parse(artifactsOutput).artifacts || [];
  const artifactNames = artifacts.map((a) => a.name);

  if (artifactNames.length === 0) {
    throw new Error(`No artifacts found in run ${runId}`);
  }

  // 4. Clean and create temp directory
  cleanTempDir();
  fs.mkdirSync(tempDir, {recursive: true});

  // 5. Check if updated-wpt-metadata artifacts are present
  const updatedMetadataArtifacts = artifactNames.filter((name) =>
    name.startsWith('updated-wpt-metadata-'),
  );

  if (updatedMetadataArtifacts.length > 0) {
    console.log('Downloading updated-wpt-metadata artifacts...');
    execFileSync(
      'gh',
      ['run', 'download', runId, '-p', 'updated-wpt-metadata-*', '-D', tempDir],
      {
        stdio: 'inherit',
      },
    );

    console.log('Merging updated-wpt-metadata...');
    const subdirs = fs.readdirSync(tempDir);
    for (const subdir of subdirs) {
      const subdirPath = join(tempDir, subdir);
      if (!fs.statSync(subdirPath).isDirectory()) continue;

      const sourceDir = join(subdirPath, 'updated-wpt-metadata');
      if (!fs.existsSync(sourceDir)) continue;

      // Copy to local wpt-metadata
      // Source structure: updated-wpt-metadata/{chromedriver,mapper}/{headless,headful}
      const kinds = fs.readdirSync(sourceDir);
      for (const kind of kinds) {
        const kindPath = join(sourceDir, kind);
        if (!fs.statSync(kindPath).isDirectory()) continue;

        const heads = fs.readdirSync(kindPath);
        for (const head of heads) {
          const headPath = join(kindPath, head);
          if (!fs.statSync(headPath).isDirectory()) continue;

          const destDir = join(process.cwd(), 'wpt-metadata', kind, head);
          console.log(`Replacing local metadata in ${destDir}`);

          if (fs.existsSync(destDir)) {
            fs.rmSync(destDir, {recursive: true, force: true});
          }
          fs.mkdirSync(destDir, {recursive: true});
          fs.cpSync(headPath, destDir, {recursive: true});
        }
      }
    }
  } else {
    // Check if raw report artifacts are present
    const reportArtifacts = artifactNames.filter((name) =>
      name.endsWith('-artifacts'),
    );
    if (reportArtifacts.length === 0) {
      throw new Error('No WPT report or updated metadata artifacts found.');
    }

    console.log('Downloading WPT report artifacts...');
    execFileSync(
      'gh',
      ['run', 'download', runId, '-p', '*-artifacts', '-D', tempDir],
      {
        stdio: 'inherit',
      },
    );

    console.log('Applying report artifacts...');
    const subdirs = fs.readdirSync(tempDir);
    const reportFiles = [];
    for (const subdir of subdirs) {
      const subdirPath = join(tempDir, subdir);
      if (!fs.statSync(subdirPath).isDirectory()) continue;

      const match = subdir.match(
        /^(cd|node)-(headless|headful)-\d+\.\d+-artifacts$/,
      );
      if (!match) continue;

      const kind = match[1];
      const head = match[2];

      const outPath = join(subdirPath, 'out');
      if (!fs.existsSync(outPath)) continue;

      const files = fs.readdirSync(outPath);
      for (const file of files) {
        if (file.startsWith('wptreport') && file.endsWith('.json')) {
          reportFiles.push({
            path: join(outPath, file),
            kind,
            head,
          });
        }
      }
    }

    if (reportFiles.length === 0) {
      throw new Error('No wptreport JSON files found in downloaded artifacts.');
    }

    for (const report of reportFiles) {
      const chromedriver = (report.kind === 'cd').toString();
      const headless = (report.head !== 'headful').toString();

      console.log(
        `Applying report: ${report.path} (kind: ${report.kind}, head: ${report.head})`,
      );

      const env = {
        ...process.env,
        CHROMEDRIVER: chromedriver,
        HEADLESS: headless,
        RUN_TESTS: 'false',
        UPDATE_EXPECTATIONS: 'true',
        VERBOSE: 'true',
        PIP_EXTRA_INDEX_URL:
          process.env.PIP_EXTRA_INDEX_URL || 'https://pypi.org/simple/',
      };

      execFileSync('node', ['tools/run-wpt.mjs', '--wpt-report', report.path], {
        stdio: 'inherit',
        env,
      });
    }
  }

  console.log('WPT expectations successfully applied!');
} finally {
  console.log('Cleaning up temporary files...');
  cleanTempDir();
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper script to facilitate faster iteration when developing
 * WebUI. Usage:
 *
 *  1) Follow the instructions at
 *     https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/webui_in_chrome.md#Load-WebUIs-straight-from-disk-experimental
 *  2) Invoke the script as follows (example Linux invocation)
 *     ./third_party/node/linux/node-linux-x64/bin/node \
 *       ./ui/webui/resources/tools/build_webui_watcher.js \
 *       -C out/<out_folder> --folder chrome/browser/resources/<webui_folder>
 *  3) Edit a file in the monitored folder, save. Observe the build
 *     automatically triggering.
 *  4) Manually reload the corresponding WebUI, it should reflect the latest
 *     code.
 */

import * as assert from 'node:assert';
import {exec, spawn} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {parseArgs, promisify} from 'node:util';

function spawnPromise(cmd, args) {
  const resolver = Promise.withResolvers();

  console.info(`[Watcher] Running \'${[cmd, ...args].join(' ')}\'`);
  const child = spawn(cmd, args);

  child.stdout.on('data', data => {
    process.stdout.write(data.toString());
  });

  child.stderr.on('data', data => {
    console.error(data.toString());
  });

  child.on('error', error => {
    console.error(`Failed to start subprocess: ${error.message}`);
  });

  child.on('close', code => {
    if (code === 0) {
      resolver.resolve();
    } else {
      console.info(`[Watcher] Child process exited with code ${code}`);
      resolver.reject(new Error(`Child process exited with code ${code}`));
    }
  });

  return resolver.promise;
}


async function main() {
  const options = {
    C: {type: 'string'},
    folder: {type: 'string'},
  };

  const args = parseArgs({options});
  const buildDir = args.values.C;

  assert.ok(args.values.folder, '-folder flag is required');
  assert.ok(args.values.C, '-C flag is required');

  const gnFolder = args.values.folder.replace(/\/$/, '');

  // Step 1: Verify targets exist.
  console.info(
      `[Watcher] Verifying build_webui() target for ${gnFolder} exists...`);
  const gnOutput = (await promisify(exec)(`gn ls ${buildDir}`, {
                     maxBuffer: 10 * 1024 * 1024,
                   })).stdout.toString();
  const allTargets = gnOutput.split('\n').filter(
      target => target.startsWith(`//${gnFolder}:`));

  const preprocessTargets =
      allTargets
          .filter(target => {
            return target.startsWith(`//${gnFolder}:preprocess_`);
          })
          .map(target => target.slice(2));
  assert.ok(preprocessTargets.length > 0);

  const buildTsTarget = `${gnFolder}:build_ts`;
  assert.ok(
      allTargets.includes(`//${buildTsTarget}`),
      `Could not find target ${buildTsTarget}`);

  // Step 2: Build 'chrome' target once
  try {
    await spawnPromise('autoninja', ['-C', buildDir, '--fast_local', 'chrome']);
    console.info('[Watcher] Initial build passed.');
  } catch (_e) {
    console.error('[Watcher] Initial build failed.');
    process.exit(1);
  }

  // Step 3: Listen for file changes.
  console.info(`[Watcher] Watching files in ${gnFolder}...`);
  console.info(`[Watcher] Press Ctrl/Cmd+C to exit.`);

  async function processChange(filename) {
    console.info(`[Watcher] Detected file change: ${filename}`);
    try {
      await spawnPromise('autoninja', [
        '-C',
        buildDir,
        '--fast_local',
        ...preprocessTargets,
        buildTsTarget,
      ]);
      console.info('[Watcher] Incremental build passed');
    } catch (_e) {
      console.error('[Watcher] Incremental build failed');
    }
  }

  let timerId = -1;
  fs.watch(gnFolder, {recursive: true}, (eventType, filename) => {
    if (eventType === 'rename') {
      return;
    }

    if (!['.html', '.ts', '.css'].some(ext => filename.endsWith(ext))) {
      return;
    }

    if (timerId !== -1) {
      clearTimeout(timerId);
    }

    // Debounce change notifications.
    timerId = setTimeout(() => {
      processChange(filename);
    }, 100);
  });
}

main();

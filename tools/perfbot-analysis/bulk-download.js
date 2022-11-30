// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const bulider = require('./builder.js');
const gsutil = require('./gsutil.js');
const tryrequire = require('./try-require.js');

const fs = require('fs');
const yargs = tryrequire.tryrequire('yargs');
if (!yargs) {
  console.error('Please install the `yargs` package from npm (`npm i yargs`).');
  return;
}

function printUpdate(msg) {
  console.log('*** ' + msg);
}

async function main() {
  if (!gsutil.exists()) {
    console.error('Command `gsutil` (used to download the files) not found.');
    console.error('You may need to install `google-cloud-sdk` package.');
    return;
  }

  const argv =
      yargs
          .option('benchmark', {
            alias: 'b',
            description: 'The benchmark to download traces for.',
            type: 'string'
          })
          .option('output', {
            alias: 'o',
            description: 'The output location for the downloaded trace files.',
            type: 'string'
          })
          .usage('Usage: $0 -b <benchmark> -o <download-path> <build-url>')
          .example(
              '$0 -b rendering.mobile -o /tmp/foo https://ci.chromium.org/ui/p/chrome/builders/ci/android-pixel2_webview-perf/24015/overview')
          .wrap(null)
          .argv;

  if (!argv.benchmark) {
    console.error('Please specify the name of a benchmark (using -b).');
    return;
  }

  if (!argv.output) {
    console.error(
        'Please specify the location to download the files to (using -o).');
    return;
  }

  if (!fs.existsSync(argv.output)) {
    console.error(`Create output location (${argv.output}) first.`);
    return;
  }

  const builder = new bulider.Build(argv._[0]);
  const task = builder.findSwarmingTask();
  printUpdate(
      `Found swarming task ${task.task_id}. Looking for child tasks ... `);
  const children = task.findChildTasks();
  printUpdate(`Found ${children.length} child tasks.`);

  const all = {};
  const promises = [];
  let taskno = 0;
  for (const child of children) {
    const p = child.downloadJSONFile('output.json');
    promises.push(p);

    p.then((output) => {
      if (argv.benchmark in output.tests) {
        const tests = Object.keys(output.tests[argv.benchmark]);
        printUpdate(
            `${++taskno}/${children.length} ${tests.length} tests found for ${
                argv.benchmark} in ${child.task_id}.`);
        for (const t of tests) {
          const artifacts = output.tests[argv.benchmark][t].artifacts;
          if (artifacts && artifacts['trace.html']) {
            const trace = artifacts['trace.html'];
            const url = (typeof (trace) === 'object' &&
                         typeof (trace[0]) === 'string') ?
                trace[0] :
                trace;
            if (t in all) {
              all[t].push(url);
            } else {
              all[t] = [url];
            }
          }
        }
      } else {
        printUpdate(`${++taskno}/${children.length} 0 tests found for ${
            argv.benchmark} in ${child.task_id}.`);
      }
    });
  }
  await Promise.all(promises);

  // Maps a gs:// url to a local filename.
  const map = {};
  const names = Object.keys(all);
  while (names.length > 0) {
    const orig = names.shift();
    const name = orig.replace('/', '_').replace('\.', '_');
    const urls = all[orig];
    if (urls.length === 1) {
      map[urls[0]] = name + '.html';
    } else {
      for (let i = 0; i < urls.length; ++i) {
        map[urls[i]] = `${name}_${i}.html`;
      }
    }
  }

  const total = Object.keys(map).length;
  printUpdate(`There are ${total} files to download.`);

  await gsutil.downloadFiles(map, argv.output);
  process.stdout.write('\nDone.\n');
}

main();

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const crbugApi = require('./crbug.js');
const tryrequire = require('../perfbot-analysis/try-require.js');
const pinpointApi = require('./pinpoint.js');

const yargs = tryrequire.tryrequire('yargs');
if (!yargs) {
  console.error('Please install the `yargs` package from npm (`npm i yargs`).');
  return;
}

function toCSV(dict) {
  const rows = ['date,link'];
  const dates = Object.keys(dict).sort(
      (a, b) => (new Date(a)).valueOf() - (new Date(b)).valueOf());
  for (const date of dates) {
    const links = [...dict[date]];
    for (const link of links) {
      rows.push(`${date},${link}`);
    }
  }
  return rows;
}

async function main() {
  const argv =
      yargs
          .option('user-email', {
            alias: 'u',
            description:
                'The email address(es) of the developer (comma-separated).',
            type: 'string',
          })
          .option('project', {
            alias: 'p',
            description:
                'The comma-separated list of projects (default: chromium).',
            type: 'string',
            default: 'chromium',
          })
          .option('since', {
            alias: 's',
            description: 'Starting date (e.g. 2021-09-01).',
          })
          .usage('Usage: $0 -u <emails> -s <since> [-p <projects>]')
          .example(
              '$0 -u linus@chromium.org,linus@google.com -s 2022-01-01 -p chromium,v8,skia')
          .wrap(null)
          .argv;
  if (!argv.u) {
    console.error(
        'Please specify the username(s) (using -u), e.g. `-u linus@chromium.org,linus@google.com`');
    return;
  }

  if (!argv.s || argv.s.split('-').length !== 3) {
    console.error(
        'Please specify the starting date (using `-s YYYY-MM-DD`), e.g. `-s 2021-09-30`');
    return;
  }

  const usernames = argv.u.split(',');

  const diary = {};
  const timestamps = new Set();
  const since = argv.s;
  const sinceDate = new Date(since);

  function addActivity(timestamp, url) {
    if (timestamp < sinceDate) {
      return;
    }

    if (timestamps.has(timestamp.valueOf())) {
      return;
    }

    const d = timestamp.toLocaleDateString();
    if (!(d in diary)) {
      diary[d] = new Set();
    }
    diary[d].add(url);
    timestamps.add(timestamp.valueOf());
  }

  const projects = argv.p.split(',');
  for (const project of projects) {
    console.log(`Exploring project: ${project} ...`);
    const crbug = new crbugApi.CrBug(`projects/${project}`);
    const users = await Promise.all(usernames.map(u => crbug.getUser(u)));
    const ids = users.map(u => u.id);

    const issues = await crbug.search(`commentby:${argv.u} modified>${since}`);

    let count = 0;
    for (const issue of issues) {
      ++count;
      process.stdout.write(`\r  Inspecting ${count}/${issues.length} issues.`);
      const comments = await crbug.getComments(issue);
      for (const comment of comments) {
        if (ids.indexOf(comment.user_id) < 0) {
          continue;
        }

        if (!comment.isActivity()) {
          continue;
        }

        addActivity(comment.timestamp, issue.url);
      }
    }
    if (issues.length === 0) {
      console.log('  No issues found.');
    } else {
      console.log('');
    }
  }

  // Now find pinpoint jobs triggered by the user against a bug.
  console.log('Looking for pinpoint jobs ...');
  const pinpoint = new pinpointApi.Pinpoint();
  for (const email of usernames) {
    const jobs = pinpoint.listJobs(email);
    console.log(`  Found ${jobs.length} jobs for ${email}.`);
    for (const job of jobs) {
      if (projects.indexOf(job.project) >= 0) {
        addActivity(job.timestamp, job.url);
      }
    }
  }

  console.log(toCSV(diary).join('\n'));
  console.log(
      'Activity score: ' +
      Object.values(diary).reduce((c, i) => c + i.size, 0));
}

main();

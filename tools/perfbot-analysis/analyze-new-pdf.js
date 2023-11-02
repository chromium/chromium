// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const extract = require('./extract-metric.js');
const tryrequire = require('./try-require.js');

const fs = require('fs');

const puppeteer = tryrequire.tryrequire('puppeteer');
if (!puppeteer) {
  console.error(
      'Please install the `puppeteer` package from npm (`npm i puppeteer`).');
  return;
}

const yargs = tryrequire.tryrequire('yargs');
if (!yargs) {
  console.error('Please install the `yargs` package from npm (`npm i yargs`).');
  return;
}

async function main() {
  const argv = yargs
                   .option('directory', {
                     alias: 'd',
                     description: 'Directory containing all the trace files.',
                     type: 'string'
                   })
                   .option('parallel', {
                     alias: 'p',
                     description: 'How many stories to process in parallel.',
                     type: 'number',
                     default: 20,
                   })
                   .wrap(null)
                   .argv;

  const location = argv.directory;
  if (!location) {
    console.error(
        'Please specify the location that contains all the traces (using -d).');
    return;
  }
  if (!fs.existsSync(location)) {
    console.error('Directotry specified (' + location + ') does not exist.');
    return;
  }

  const filenames = fs.readdirSync(location).filter(f => f.endsWith('.html'));
  const browser = await puppeteer.launch();
  const promises = [];
  let counter = 1;
  const rows = [['name', 'PercentDroppedFrames', 'PercentDroppedFrames2']];
  for (const f of filenames) {
    const traceurl = 'file://' + fs.realpathSync(`${location}/${f}`);
    const p = extract.extractMetrics(browser, traceurl, 'umaMetric');
    promises.push(p);
    p.then((histograms) => {
      const oldh = 'Graphics.Smoothness.PercentDroppedFrames.AllSequences';
      const newh = 'Graphics.Smoothness.PercentDroppedFrames2.AllSequences';
      let msg = '';

      if ('error' in histograms) {
        msg = 'some error happened: ' + histograms.error;
      } else if (!(oldh in histograms) && !(newh in histograms)) {
        msg = 'no metrics.';
      } else {
        if (!(newh in histograms)) {
          const oldv = histograms[oldh].avg;
          msg = `No new metric, old metric: ${oldv.toFixed(2)}`;
          rows.push([f, oldv, -1]);
        } else if (!(oldh in histograms)) {
          const newv = histograms[newh].avg;
          msg = `No old metric, new metric: ${newv.toFixed(2)}`;
          rows.push([f, -1, newv]);
        } else {
          const newv = histograms[newh].avg;
          const oldv = histograms[oldh].avg;
          const diff = Math.abs(newv - oldv);
          msg = `Difference: ${diff.toFixed(2)} (${newv.toFixed(2)} vs ${
              oldv.toFixed(2)}).`;
          rows.push([f, oldv, newv]);
        }
      }

      console.log(`${counter++}/${filenames.length} ${f}: ${msg}`);
    });

    if (promises.length === argv.p) {
      await Promise.all(promises);
      promises.length = 0;
    }
  }
  await Promise.all(promises);
  await browser.close();

  console.log(rows.map(r => r.join(',')).join('\n'));
}

main();

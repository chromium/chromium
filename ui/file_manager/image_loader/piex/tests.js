// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const example = 'node tests tests.html [options]';

const program = require('commander');

program.usage('test-page [options]')
  .description('piex wasm raw image preview test runner')
  .option('-d, --debug', 'enable debug mode');

program.on('--help', () => {
  console.info('');
  console.info('  % ' + example);
  console.info('');
});

program.explain = () => {
  return undefined !== process.argv.find((element) => {
    return element === '--help' || element === '-h';
  });
};

program.parse(process.argv);
if (!program.args.length || program.explain()) {
  program.help();
  process.exit(1);
}

process.on('unhandledRejection', (error) => {
  console.info('unhandledRejection', error);
  process.exit(1);
});

const server = require('http-server').createServer();
server.listen(8123);

const puppeteer = require('puppeteer');

(async function main() {
  if (program.debug) {
    console.info(puppeteer.defaultArgs());
  }

  let args = [];
  if (process.platform === 'linux') {
    args = ['--no-sandbox'];
  }

  const browser = await puppeteer.launch({
    headless: !program.debug,
    args: [...args],
  });

  const page = await browser.newPage();

  await page.setViewport({
    width: 1200,
    height: 800,
  });

  if (program.debug) {
    page.on('request', (request) => {
      console.info('Request: ', request.url());
    });

    page.on('close', () => {
      process.exit(1);
    });
  }

  page.on('console', (message) => {
    console.info(message.text());
  });

  await page.goto('http://localhost:8123/' + process.argv[2], {
    waitUntil: 'networkidle2',
  });

  await page.mainFrame().waitForFunction('document.title === "READY"');

  const sleep = (time) => {
    return new Promise(resolve => setTimeout(resolve, time));
  };

  const images = [
    'images/SONY_A500_01.ARW',
    'images/EOS_XS_REBEL.CR2',
    'images/RAW_CANON_1DM2.CR2',
    'images/L100_4220.DNG',
    'images/RAW_LEICA_M8.DNG',
    'images/FUJI_E550_RAW.RAF',
    'images/NIKON_UB20_O35.NEF',
    'images/NIKON_GDN0447.NEF',
    'images/OLYMPUS_SC877.ORF',
    'images/NIKON_CPIX78.NRW',
    'images/PANASONIC_DMC.RW2',
    'images/UNKNOWN_FORMAT.JPG',
  ];

  await page.evaluate((length) => {
    return window.createFileSystem(length);
  }, images.length);

  await Promise.all(images.map((image) => {
    return page.evaluate((image) => {
      return window.writeToFileSystem(image);
    }, image);
  }));

  await page.evaluate(() => {
    window.testTime = 0;
  });

  for (let i = 0; i < images.length; ++i) {
    await page.evaluate((image) => {
      return window.runTest(image);
    }, images[i]);

    await page.mainFrame().waitForFunction('document.title === "DONE"');

    if (program.debug) {
      await sleep(2000);
    }
  }

  await page.evaluate(() => {
    console.info('test: done total time', window.testTime.toFixed(3));
  });

  browser.close();
  server.close();

})();

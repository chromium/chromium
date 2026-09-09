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

import fs from 'fs';
import {parseArgs} from 'node:util';

import {
  calculateStats,
  printStats,
  printComparison,
  printCiComparison,
  RUNS,
  ITERATIONS_PER_RUN,
  WARMUP_ITERATIONS,
  BENCHMARK_HTML,
  getChromePath,
  getChromeDriverPath,
  getBidiMapperPath,
} from './benchmark-utils.mjs';

const {values} = parseArgs({
  options: {
    runner: {
      type: 'string',
      default: 'puppeteer',
    },
  },
  strict: false,
});

async function runPuppeteerIteration(launchOptions, chromePath) {
  const puppeteer = (await import('puppeteer')).default;
  const browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: 'shell',
    ...launchOptions,
  });

  try {
    const page = await browser.newPage();
    await page.goto('about:blank');
    await page.setContent(BENCHMARK_HTML);

    for (let i = 0; i < WARMUP_ITERATIONS; i++) {
      await page.evaluate(
        (index, id) => {
          document.getElementById(id).innerText = `Warmup: ${index + 1}`;
        },
        i,
        'some-counter',
      );
    }

    const latencies = [];
    for (let i = 0; i < ITERATIONS_PER_RUN; i++) {
      const start = performance.now();
      await page.evaluate(
        (index, id) => {
          document.getElementById(id).innerText = `Iter: ${index + 1}`;
          return {
            iteration: index + 1,
            timestamp: Date.now(),
            someArray: [1, 2, 3],
            someObject: {a: 1, b: 2, c: 3},
            someString: 'hello',
            someNumber: 123,
            someBoolean: true,
            someNull: null,
            someUndefined: undefined,
          };
        },
        i,
        'some-counter',
      );
      const end = performance.now();
      latencies.push(end - start);
    }
    return latencies;
  } finally {
    await browser.close();
  }
}

async function runPuppeteerBenchmark() {
  const nestedChromiumBidiPaths = [
    'node_modules/puppeteer-core/node_modules/chromium-bidi',
    'node_modules/puppeteer/node_modules/chromium-bidi',
  ];
  for (const nestedPath of nestedChromiumBidiPaths) {
    if (fs.existsSync(nestedPath)) {
      console.log(
        `Removing nested chromium-bidi dependency at ${nestedPath}...`,
      );
      fs.rmSync(nestedPath, {recursive: true, force: true});
    }
  }

  const chromePath = getChromePath();
  const bidiMapperPath = getBidiMapperPath();
  console.log(`Using Chrome: ${chromePath}`);
  console.log(`Using BiDi Mapper: ${bidiMapperPath}`);
  console.log(
    `Starting Puppeteer Benchmark: ${RUNS} runs x ${ITERATIONS_PER_RUN} iterations...`,
  );

  const stats = {cdp: [], bidi: []};

  for (let i = 0; i < RUNS; i++) {
    process.stdout.write(`Run ${i + 1}/${RUNS}: `);

    process.stdout.write(`running CDP... `);
    const cdpLatencies = await runPuppeteerIteration({}, chromePath);
    stats.cdp.push(...cdpLatencies);

    process.stdout.write(`running BiDi... `);
    const bidiLatencies = await runPuppeteerIteration(
      {protocol: 'webDriverBiDi'},
      chromePath,
    );
    stats.bidi.push(...bidiLatencies);

    console.log('Done.');
  }

  const cdpFinal = calculateStats(stats.cdp);
  const bidiFinal = calculateStats(stats.bidi);

  console.log('\n=== Results (Mean of Runs) ===');
  printStats('Puppeteer CDP', cdpFinal);
  printStats('Puppeteer BiDi', bidiFinal);
  console.log('\n=== Comparison (BiDi vs CDP) ===');
  printComparison(bidiFinal, cdpFinal);
  printCiComparison('diff', bidiFinal, cdpFinal, 'puppeteer');
}

async function runSeleniumIteration(mode, i, context, benchmarkAction) {
  const script = `(${benchmarkAction.toString()})(${i}, 'some-counter')`;
  if (mode === 'classic') {
    await context.driver.executeScript(script);
  } else if (mode === 'cdp') {
    await context.cdpSession.send('Runtime.evaluate', {
      expression: script,
      serializationOptions: {serialization: 'deep'},
    });
  } else if (mode === 'bidi') {
    await context.scriptManager.callFunctionInBrowsingContext(
      context.bidiContextId,
      script,
      false,
    );
  }
}

async function runSeleniumBenchmark() {
  const {Builder, ScriptManager} = await import('selenium-webdriver');
  const chrome = (await import('selenium-webdriver/chrome.js')).default;

  const chromePath = getChromePath();
  const chromeDriverPath = getChromeDriverPath();
  const bidiMapperPath = getBidiMapperPath();

  console.log(`Using Headless Shell: ${chromePath}`);
  console.log(`Using ChromeDriver: ${chromeDriverPath}`);
  console.log(`Using BiDi Mapper: ${bidiMapperPath}`);
  console.log(
    `Starting Selenium Benchmark: ${RUNS} runs x ${ITERATIONS_PER_RUN} iterations...`,
  );

  const runRun = async (mode) => {
    const service = new chrome.ServiceBuilder(chromeDriverPath).addArguments(
      `--bidi-mapper-path=${bidiMapperPath}`,
    );
    const options = new chrome.Options()
      .setChromeBinaryPath(chromePath)
      .addArguments('--headless=new');

    if (mode === 'bidi') {
      options.enableBidi();
    }

    const driver = await new Builder()
      .forBrowser('chrome')
      .setChromeOptions(options)
      .setChromeService(service)
      .build();

    try {
      let bidiContextId;
      let cdpSession;
      let scriptManager;

      if (mode === 'cdp') {
        cdpSession = await driver.createCDPConnection('page');
      } else if (mode === 'bidi') {
        bidiContextId = await driver.getWindowHandle();
        scriptManager = await ScriptManager(bidiContextId, driver);
      }

      await driver.get(`data:text/html,${encodeURIComponent(BENCHMARK_HTML)}`);

      const benchmarkAction = (index, id) => {
        document.getElementById(id).innerText = `Iter: ${index + 1}`;
        return {iteration: index + 1, timestamp: Date.now()};
      };

      for (let i = 0; i < WARMUP_ITERATIONS; i++) {
        await runSeleniumIteration(
          mode,
          i,
          {driver, cdpSession, scriptManager, bidiContextId},
          benchmarkAction,
        );
      }

      const latencies = [];
      for (let i = 0; i < ITERATIONS_PER_RUN; i++) {
        const start = performance.now();
        await runSeleniumIteration(
          mode,
          i,
          {driver, cdpSession, scriptManager, bidiContextId},
          benchmarkAction,
        );
        latencies.push(performance.now() - start);
      }
      return latencies;
    } finally {
      await driver.quit();
    }
  };

  const stats = {classic: [], cdp: [], bidi: []};
  for (let i = 0; i < RUNS; i++) {
    process.stdout.write(`Run ${i + 1}/${RUNS}: `);
    process.stdout.write(`running Classic... `);
    stats.classic.push(...(await runRun('classic')));
    process.stdout.write(`running CDP... `);
    stats.cdp.push(...(await runRun('cdp')));
    process.stdout.write(`running BiDi... `);
    stats.bidi.push(...(await runRun('bidi')));
    console.log('Done.');
  }

  const classicFinal = calculateStats(stats.classic);
  const cdpFinal = calculateStats(stats.cdp);
  const bidiFinal = calculateStats(stats.bidi);

  console.log('\n=== Results (Mean of Runs) ===');
  printStats('Selenium Classic', classicFinal);
  printStats('Selenium CDP', cdpFinal);
  printStats('Selenium BiDi', bidiFinal);
  console.log('\n=== Comparison (BiDi vs Classic) ===');
  printComparison(bidiFinal, classicFinal);
  printCiComparison('diff', bidiFinal, classicFinal, 'selenium');
}

async function main() {
  const runner = values.runner || 'puppeteer';
  if (runner === 'puppeteer') {
    await runPuppeteerBenchmark();
  } else if (runner === 'selenium') {
    await runSeleniumBenchmark();
  } else if (runner === 'all') {
    await runPuppeteerBenchmark();
    await runSeleniumBenchmark();
  } else {
    console.error(
      `Unknown runner: ${runner}. Expected 'puppeteer', 'selenium', or 'all'.`,
    );
    process.exit(1);
  }
}

await main();

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

import {Builder, ScriptManager} from 'selenium-webdriver';
import chrome from 'selenium-webdriver/chrome.js';

import {
  calculateStats,
  printStats,
  printComparison,
  printCiComparison,
  RUNS,
  ITERATIONS_PER_RUN,
  WARMUP_ITERATIONS,
  BENCHMARK_HTML,
} from './benchmark-utils.mjs';
import {
  installAndGetChromePath,
  installAndGetChromeDriverPath,
  getBidiMapperPath,
} from './path-getter/path-getter.mjs';

async function runIteration(mode, i, context, benchmarkAction) {
  const script = `(${benchmarkAction.toString()})(${i}, 'some-counter')`;
  if (mode === 'classic') {
    await context.driver.executeScript(script);
    // await context.driver.executeScript(benchmarkAction, i, 'some-counter');
  } else if (mode === 'cdp') {
    await context.cdpSession.send('Runtime.evaluate', {
      expression: script,
      serializationOptions: {serialization: 'deep'},
    });
  } else if (mode === 'bidi') {
    await context.scriptManager.callFunctionInBrowsingContext(
      context.bidiContextId,
      script,
      false, // awaitPromise
    );
  }
}

async function runBenchmarkRun(
  mode,
  chromePath,
  chromeDriverPath,
  bidiMapperPath,
) {
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

    const url = `data:text/html,${encodeURIComponent(BENCHMARK_HTML)}`;
    await driver.get(url);

    const benchmarkAction = (index, id) => {
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
    };

    // Warmup.
    for (let i = 0; i < WARMUP_ITERATIONS; i++) {
      await runIteration(
        mode,
        i,
        {driver, cdpSession, scriptManager, bidiContextId},
        benchmarkAction,
      );
    }

    const latencies = [];
    for (let i = 0; i < ITERATIONS_PER_RUN; i++) {
      const start = performance.now();
      await runIteration(
        mode,
        i,
        {driver, cdpSession, scriptManager, bidiContextId},
        benchmarkAction,
      );
      const end = performance.now();
      latencies.push(end - start);
    }
    return latencies;
  } finally {
    await driver.quit();
  }
}

async function main() {
  const chromePath = installAndGetChromePath(true);
  const chromeDriverPath = installAndGetChromeDriverPath();
  const bidiMapperPath = getBidiMapperPath();

  console.log(`Using Headless Shell: ${chromePath}`);
  console.log(`Using ChromeDriver: ${chromeDriverPath}`);
  console.log(`Using BiDi Mapper: ${bidiMapperPath}`);

  console.log(
    `Starting Benchmark: ${RUNS} runs x ${ITERATIONS_PER_RUN} iterations...`,
  );

  const stats = {
    cdp: [],
    classic: [],
    bidi: [],
  };

  for (let i = 0; i < RUNS; i++) {
    process.stdout.write(`Run ${i + 1}/${RUNS}: `);

    // Run Classic
    process.stdout.write(`running Classic... `);
    const classicLatencies = await runBenchmarkRun(
      'classic',
      chromePath,
      chromeDriverPath,
      bidiMapperPath,
    );
    stats.classic.push(...classicLatencies);

    // Run CDP
    process.stdout.write(`running CDP... `);
    const cdpLatencies = await runBenchmarkRun(
      'cdp',
      chromePath,
      chromeDriverPath,
      bidiMapperPath,
    );
    stats.cdp.push(...cdpLatencies);

    // Run BiDi
    process.stdout.write(`running BiDi... `);
    const bidiLatencies = await runBenchmarkRun(
      'bidi',
      chromePath,
      chromeDriverPath,
      bidiMapperPath,
    );
    stats.bidi.push(...bidiLatencies);

    console.log('Done.');
  }

  const classicFinal = calculateStats(stats.classic);
  const cdpFinal = calculateStats(stats.cdp);
  const bidiFinal = calculateStats(stats.bidi);

  console.log('\n=== Results (Mean of Runs) ===');
  printStats('Selenium Classic', classicFinal);
  printStats('Selenium CDP', cdpFinal);
  printStats('Selenium BiDi', bidiFinal);

  // Comparisons
  console.log('\n=== Comparison (Classic vs CDP) ===');
  printComparison(classicFinal, cdpFinal);

  console.log('\n=== Comparison (BiDi vs CDP) ===');
  printComparison(bidiFinal, cdpFinal);

  // Print metrics to the file for CI.
  printCiComparison('classic_diff', classicFinal, cdpFinal, 'selenium');
  printCiComparison('bidi_diff', bidiFinal, cdpFinal, 'selenium');
}

await main();

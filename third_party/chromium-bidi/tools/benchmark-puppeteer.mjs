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

import puppeteer from 'puppeteer';

import {installAndGetChromePath} from './path-getter/path-getter.mjs';

const ITERATIONS = 10000;
const WARMUP_ITERATIONS = ITERATIONS * 0.1;

function calculateStats(latencies) {
  latencies.sort((a, b) => a - b);
  // Filter out the slowest 1% to improve stability (remove outliers like GC pauses).
  const sorted = latencies.slice(0, Math.floor(latencies.length * 0.99));
  const count = sorted.length;
  const totalTime = sorted.reduce((sum, val) => sum + val, 0);
  const averageTime = totalTime / count;
  const p50 = sorted[Math.floor(count * 0.5)];
  const p90 = sorted[Math.floor(count * 0.9)];
  const min = sorted[0];
  const max = sorted[count - 1];

  const variance =
    sorted.reduce((sum, val) => sum + Math.pow(val - averageTime, 2), 0) /
    count;
  const stdDev = Math.sqrt(variance);
  const rsd = (stdDev / averageTime) * 100;

  // Calculate 95% Confidence Interval (Z ≈ 1.96)
  const standardError = stdDev / Math.sqrt(count);
  const marginOfError = 1.96 * standardError;
  const ciRsd = (marginOfError / averageTime) * 100;

  return {
    totalTime,
    averageTime,
    min,
    max,
    p50,
    p90,
    stdDev,
    rsd,
    standardError,
    marginOfError,
    ciRsd,
  };
}

async function runBenchmark(name, launchOptions) {
  const executablePath = installAndGetChromePath();
  console.log(`\n=== Testing Browser: ${name} ===`);
  console.log(`Using Chrome: ${executablePath}`);

  const browser = await puppeteer.launch({
    executablePath,
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
    headless: true,
    ...launchOptions,
  });

  try {
    const page = await browser.newPage();
    await page.goto('about:blank');

    // Setup DOM
    await page.evaluate((browserName) => {
      document.body.innerHTML = `
        <div style='font-family:Segoe UI, sans-serif; padding:20px; background:#f4f7f6;'>
          <h2>${browserName} Protocol Benchmark</h2>
          <div style='display:flex; gap:15px;'>
            <div id='some-box' style='flex:1; padding:15px; background:white; border-left:5px solid #3498db;'>Some counter<div id='some-counter' style='font-size:24px;'>0</div><div id='some-res'>-</div></div>
          </div>
        </div>`;
    }, name);

    console.log(`Warming up for ${WARMUP_ITERATIONS} iterations...`);
    for (let i = 0; i < WARMUP_ITERATIONS; i++) {
      await page.evaluate(
        (index, id) => {
          document.getElementById(id).innerText = `Warmup: ${index + 1}`;
        },
        i,
        'some-counter',
      );
    }
    console.log('Warmup complete.');

    const latencies = [];
    console.log('Starting measurement...');

    for (let i = 0; i < ITERATIONS; i++) {
      const startIteration = performance.now();
      await page.evaluate(
        (index, id) => {
          document.getElementById(id).innerText = `Iter: ${index + 1}`;
        },
        i,
        'some-counter',
      );
      const endIteration = performance.now();
      latencies.push(endIteration - startIteration);
    }

    const stats = calculateStats(latencies);

    console.log(
      `Total time for ${ITERATIONS} iterations: ${stats.totalTime.toFixed(2)}ms`,
    );
    console.log(
      `Average script execution time: ${stats.averageTime.toFixed(4)}ms`,
    );
    console.log(`Min script execution time: ${stats.min.toFixed(4)}ms`);
    console.log(`Max script execution time: ${stats.max.toFixed(4)}ms`);
    console.log(`p50 script execution time: ${stats.p50.toFixed(4)}ms`);
    console.log(`p90 script execution time: ${stats.p90.toFixed(4)}ms`);
    console.log(
      `Standard Deviation: ${stats.stdDev.toFixed(4)}ms (${stats.rsd.toFixed(2)}%)`,
    );
    console.log(`95% Confidence Interval: ±${stats.ciRsd.toFixed(2)}%`);

    return stats;
  } finally {
    await browser.close();
  }
}

const cdpStats = await runBenchmark('Puppeteer CDP', {});
const bidiStats = await runBenchmark('Puppeteer BiDi', {
  protocol: 'webDriverBiDi',
});

console.log('\n=== Comparison (BiDi vs CDP) ===');
const diff = bidiStats.averageTime - cdpStats.averageTime;
const diffPercent = (diff / cdpStats.averageTime) * 100;

// Standard Error of Difference = sqrt(SE1^2 + SE2^2) (assuming independent samples)
const seDiff = Math.sqrt(
  Math.pow(cdpStats.standardError, 2) + Math.pow(bidiStats.standardError, 2),
);
const moeDiff = 1.96 * seDiff;
const moeDiffPercent = (moeDiff / cdpStats.averageTime) * 100;
const ciLowerPercent = ((diff - moeDiff) / cdpStats.averageTime) * 100;
const ciUpperPercent = ((diff + moeDiff) / cdpStats.averageTime) * 100;

const slowerOrFaster = diff >= 0 ? 'slower' : 'faster';

console.log(
  `BiDi is ${Math.abs(diff).toFixed(4)}ms (${Math.abs(diffPercent).toFixed(4)}%) ${slowerOrFaster} than CDP`,
);
console.log(
  `95% Confidence Interval of the difference: [${ciLowerPercent.toFixed(2)}%, ${ciUpperPercent.toFixed(2)}%] (±${moeDiffPercent.toFixed(2)}%)`,
);

console.log(`PERF_METRIC:diff_bidi_vs_cdp:${diffPercent.toFixed(4)}`);

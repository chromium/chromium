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

const RUNS = 100;
const ITERATIONS_PER_RUN = 100;
const WARMUP_ITERATIONS = Math.max(2, 0.1 * ITERATIONS_PER_RUN);

// For large sample sizes (N > 1000), the t-distribution converges to the normal distribution.
// 1.96 is the approximate critical value for a 95% Confidence Interval (Z-score).
const T_CRIT_95_LARGE_N = 1.96;

const BENCHMARK_HTML = `
<div style='font-family:Segoe UI, sans-serif; padding:20px; background:#f4f7f6;'>
  <h2>Protocol Benchmark</h2>
  <div style='display:flex; gap:15px;'>
    <div id='some-box' style='flex:1; padding:15px; background:white; border-left:5px solid #3498db;'>
      Some counter<div id='some-counter' style='font-size:24px;'>0</div><div id='some-res'>-</div>
    </div>
  </div>
</div>`;

/**
 * Calculates statistical metrics for a set of benchmark run means.
 *
 * This function processes an array of latencies to compute central tendency (mean),
 * dispersion (variance, stdDev), and reliability estimates (confidence intervals).
 */
function calculateStats(latencies) {
  latencies.sort((a, b) => a - b);
  const count = latencies.length;
  const totalTime = latencies.reduce((sum, val) => sum + val, 0);

  // Arithmetic Mean (x̄): The central tendency of the run means.
  // x̄ = (Σx) / n
  const averageTime = totalTime / count;
  const min = latencies[0];
  const max = latencies[count - 1];

  // Sample Variance (s²): Measures the spread of the run means around the average.
  // We use Sample Variance (divided by n-1) instead of Population Variance (divided by n)
  // because we are estimating the true variance from a small sample of runs (Bessel's
  // correction).
  // s² = Σ(x - x̄)² / (n - 1)
  const variance =
    latencies.reduce((sum, val) => sum + Math.pow(val - averageTime, 2), 0) /
    (count - 1);

  // Sample Standard Deviation (s): The square root of the variance, representing spread
  // in the same units as the data (ms).
  // s = √s²
  const stdDev = Math.sqrt(variance);

  // Relative Standard Deviation (RSD), also known as Coefficient of Variation (CV).
  // Expresses standard deviation as a percentage of the mean, allowing comparison of
  // variability across different scales.
  // RSD = (s / x̄) * 100
  const rsd = (stdDev / averageTime) * 100;

  // Standard Error of the Mean (SE): Estimates how far the sample mean (x̄) is likely to
  // be from the true population mean (μ).
  // SE = s / √n
  const standardError = stdDev / Math.sqrt(count);

  // Critical Value for 95% Confidence Interval.
  // Since the sample size is large (N > 1000), the T-distribution converges to the Normal
  // (Z) distribution. We use the standard Z-score of 1.96.
  const tCrit = T_CRIT_95_LARGE_N;

  // Margin of Error (ME): The half-width of the Confidence Interval.
  // ME = t_crit * SE
  const marginOfError = tCrit * standardError;

  // CI Relative Standard Deviation (CI RSD): Margin of Error derived as a percentage of
  // the mean.
  const ciRsd = (marginOfError / averageTime) * 100;

  return {
    averageTime,
    min,
    max,
    stdDev,
    rsd,
    standardError,
    marginOfError,
    ciRsd,
  };
}

async function runBenchmarkRun(name, launchOptions, chromePath) {
  const browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: true,
    ...launchOptions,
  });

  try {
    const page = await browser.newPage();
    await page.goto('about:blank');
    await page.setContent(BENCHMARK_HTML);

    // Warmup
    for (let i = 0; i < WARMUP_ITERATIONS; i++) {
      await page.evaluate(
        (index, id) => {
          document.getElementById(id).innerText = `Warmup: ${index + 1}`;
        },
        i,
        'some-counter',
      );
    }

    // Measurement
    const latencies = [];
    for (let i = 0; i < ITERATIONS_PER_RUN; i++) {
      const start = performance.now();
      await page.evaluate(
        (index, id) => {
          // Change DOM.
          document.getElementById(id).innerText = `Iter: ${index + 1}`;
          // Return an object.
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

    // Return all latencies for this run
    return latencies;
  } finally {
    await browser.close();
  }
}

async function main() {
  // Headless shell does not work with Puppeteer. Use new headless.
  const chromePath = installAndGetChromePath();
  console.log(`Using Chrome: ${chromePath}`);
  console.log(
    `Starting Benchmark: ${RUNS} runs x ${ITERATIONS_PER_RUN} iterations...`,
  );

  const stats = {
    cdp: [],
    bidi: [],
  };

  for (let i = 0; i < RUNS; i++) {
    process.stdout.write(`Run ${i + 1}/${RUNS}: `);

    // Run CDP.
    process.stdout.write(`running CDP... `);
    const cdpLatencies = await runBenchmarkRun('Puppeteer CDP', {}, chromePath);
    stats.cdp.push(...cdpLatencies);

    process.stdout.write(`running BiDi... `);
    const bidiLatencies = await runBenchmarkRun(
      'Puppeteer BiDi',
      {protocol: 'webDriverBiDi'},
      chromePath,
    );
    stats.bidi.push(...bidiLatencies);

    console.log('Done.');
  }

  const cdpFinal = calculateStats(stats.cdp);
  const bidiFinal = calculateStats(stats.bidi);

  console.log('\n=== Results (Mean of Runs) ===');
  console.log('Puppeteer CDP:');
  console.log(`  Mean: ${cdpFinal.averageTime.toFixed(4)}ms`);
  console.log(
    `  95% CI: ±${cdpFinal.marginOfError.toFixed(4)}ms (±${cdpFinal.ciRsd.toFixed(2)}%)`,
  );
  console.log('Puppeteer BiDi:');
  console.log(`  Mean: ${bidiFinal.averageTime.toFixed(4)}ms`);
  console.log(
    `  95% CI: ±${bidiFinal.marginOfError.toFixed(4)}ms (±${bidiFinal.ciRsd.toFixed(2)}%)`,
  );

  console.log('\n=== Comparison (BiDi vs CDP) ===');
  const diff = bidiFinal.averageTime - cdpFinal.averageTime;
  const diffPercent = (diff / cdpFinal.averageTime) * 100;

  // Standard Error of the Difference (SE_diff).
  // Formula: SE_diff = sqrt(SE_cdp^2 + SE_bidi^2)
  // Assumes independent samples and unequal variances (unpooled SE).
  const seDiff = Math.sqrt(
    Math.pow(cdpFinal.standardError, 2) + Math.pow(bidiFinal.standardError, 2),
  );

  // Critical Value (t_crit) from Student's t-distribution for 95% Confidence.
  // Degrees of Freedom (df) is conservatively estimated as min(N1-1, N2-1).
  // With large N (e.g. 1000), this will be 1.96.
  const tCrit = T_CRIT_95_LARGE_N;

  // Margin of Error for the Difference (ME_diff).
  // Formula: ME_diff = t_crit * SE_diff
  const moeDiff = tCrit * seDiff;

  // Margin of Error as a percentage of the baseline (CDP) average time.
  const moeDiffPercent = (moeDiff / cdpFinal.averageTime) * 100;

  const slowerOrFaster = diff >= 0 ? 'slower' : 'faster';

  console.log(
    `BiDi is ${Math.abs(diff).toFixed(4)}ms ±${moeDiff.toFixed(4)}ms (${Math.abs(diffPercent).toFixed(4)}% ±${moeDiffPercent.toFixed(2)}%) ${slowerOrFaster} than CDP`,
  );

  console.log(`PERF_METRIC:VALUE:${diffPercent.toFixed(4)}`);
  console.log(`PERF_METRIC:RANGE:${moeDiffPercent.toFixed(4)}`);
}

await main();

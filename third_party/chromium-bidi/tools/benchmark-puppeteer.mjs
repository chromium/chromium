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
  const totalSum = latencies.reduce((sum, val) => sum + val, 0);

  // Arithmetic Mean (x̄): The sum of all measurements divided by the count.
  // x̄ = (Σx) / n
  const mean = totalSum / count;
  const min = latencies[0];
  const max = latencies[count - 1];

  // Median: The value separating the higher half from the lower half of the data samples.
  let median;
  if (count % 2 === 0) {
    median = (latencies[count / 2 - 1] + latencies[count / 2]) / 2;
  } else {
    median = latencies[Math.floor(count / 2)];
  }

  // Sample Variance (s²): Unbiased estimator of the population variance.
  // Uses Bessel's correction (n - 1) because we are estimating the true variance
  // from a sample, not the entire population.
  // s² = Σ(x - x̄)² / (n - 1)
  const variance =
    latencies.reduce((sum, val) => sum + Math.pow(val - mean, 2), 0) /
    (count - 1);

  // Sample Standard Deviation (s): The square root of the sample variance.
  // Represents the dispersion of the data in the same units as the data itself.
  // s = √s²
  const stdDev = Math.sqrt(variance);

  // Coefficient of Variation (CV) / Relative Standard Deviation (RSD):
  // The ratio of the standard deviation to the mean, expressed as a percentage.
  // Useful for comparing the degree of variation from one data series to another,
  // even if the means are drastically different.
  // RSD = (s / x̄) * 100
  const rsd = (stdDev / mean) * 100;

  // Standard Error of the Mean (SE_mean): The standard deviation of the sampling
  // distribution of the sample mean. Estimates the precision of the sample mean
  // as an estimate of the population mean.
  // SE_mean = s / √n
  const standardErrorMean = stdDev / Math.sqrt(count);

  // Critical Value (z*) for 95% Confidence Interval:
  // For a large sample size (n >= 30), the sampling distribution of the mean is
  // approximately normal (Central Limit Theorem). We use the Z-score 1.96 for 95% confidence.
  const tCrit = T_CRIT_95_LARGE_N;

  // Margin of Error (MOE) for the Mean:
  // The radius of the confidence interval. We can be 95% confident that the true
  // population mean lies within x̄ ± MOE.
  // MOE_mean = z* * SE_mean
  const marginOfErrorMean = tCrit * standardErrorMean;

  // CI Relative Standard Deviation (CI RSD) for Mean:
  // The Margin of Error expressed as a percentage of the mean.
  const ciRsdMean = (marginOfErrorMean / mean) * 100;

  // Standard Error of the Median (SE_med) approximation:
  // Under the assumption of a Normal distribution, the asymptotic variance of the median
  // is (π/2) * (σ^2 / n). Thus, SE_med ≈ SE_mean * √1.57 ≈ 1.253 * SE_mean.
  const standardErrorMedian = 1.253 * standardErrorMean;

  // Margin of Error (MOE) for the Median:
  // MOE_med = z* * SE_med
  const marginOfErrorMedian = tCrit * standardErrorMedian;

  // CI Relative Standard Deviation (CI RSD) for Median:
  // The Margin of Error expressed as a percentage of the median.
  const ciRsdMedian = (marginOfErrorMedian / median) * 100;

  return {
    mean,
    median,
    min,
    max,
    stdDev,
    rsd,
    standardErrorMean,
    marginOfErrorMean,
    ciRsdMean,
    standardErrorMedian,
    marginOfErrorMedian,
    ciRsdMedian,
  };
}

async function runBenchmarkRun(launchOptions, chromePath) {
  const browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: 'shell',
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
  const chromePath = installAndGetChromePath(true);
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
    const cdpLatencies = await runBenchmarkRun({}, chromePath);
    stats.cdp.push(...cdpLatencies);

    process.stdout.write(`running BiDi... `);
    const bidiLatencies = await runBenchmarkRun(
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
  console.log(
    `  Mean: ${cdpFinal.mean.toFixed(4)}ms ±${cdpFinal.marginOfErrorMean.toFixed(4)}ms (±${cdpFinal.ciRsdMean.toFixed(2)}%)`,
  );
  console.log(
    `  Median: ${cdpFinal.median.toFixed(4)}ms ±${cdpFinal.marginOfErrorMedian.toFixed(4)}ms (±${cdpFinal.ciRsdMedian.toFixed(2)}%)`,
  );

  console.log('Puppeteer BiDi:');
  console.log(
    `  Mean: ${bidiFinal.mean.toFixed(4)}ms ±${bidiFinal.marginOfErrorMean.toFixed(4)}ms (±${bidiFinal.ciRsdMean.toFixed(2)}%)`,
  );
  console.log(
    `  Median: ${bidiFinal.median.toFixed(4)}ms ±${bidiFinal.marginOfErrorMedian.toFixed(4)}ms (±${bidiFinal.ciRsdMedian.toFixed(2)}%)`,
  );

  console.log('\n=== Comparison (BiDi vs CDP) ===');
  const meanDiffAbs = bidiFinal.mean - cdpFinal.mean;
  const meanDiffRel = (meanDiffAbs / cdpFinal.mean) * 100;

  // Standard Error of the Difference (SE_diff).
  // Formula: SE_diff = sqrt(SE_cdp^2 + SE_bidi^2)
  // Assumes independent samples and unequal variances (unpooled SE).
  const meanDiffStandardError = Math.sqrt(
    Math.pow(cdpFinal.standardErrorMean, 2) +
      Math.pow(bidiFinal.standardErrorMean, 2),
  );

  // Margin of Error for the Difference (ME_diff).
  // Formula: ME_diff = t_crit * SE_diff
  const meanDiffAbsMoe = meanDiffStandardError * T_CRIT_95_LARGE_N;
  // Margin of Error as a percentage of the baseline (CDP) mean time.
  const meanDiffRelMoe = (meanDiffAbsMoe / cdpFinal.mean) * 100;

  const medianDiffAbs = bidiFinal.median - cdpFinal.median;
  const medianDiffRel = (medianDiffAbs / cdpFinal.median) * 100;

  // Standard Error of the Difference (SE_diff).
  // Formula: SE_diff = sqrt(SE_cdp^2 + SE_bidi^2)
  // Assumes independent samples and unequal variances (unpooled SE).
  const medianDiffStandardError = Math.sqrt(
    Math.pow(cdpFinal.standardErrorMedian, 2) +
      Math.pow(bidiFinal.standardErrorMedian, 2),
  );

  // Margin of Error for the Difference (ME_diff).
  // Formula: ME_diff = t_crit * SE_diff
  const medianDiffAbsMoe = medianDiffStandardError * T_CRIT_95_LARGE_N;
  // Margin of Error as a percentage of the baseline (CDP) Median time.
  const medianDiffRelMoe = (medianDiffAbsMoe / cdpFinal.median) * 100;

  console.log(
    `  Mean:   ${meanDiffRel.toFixed(2)}% ±${meanDiffRelMoe.toFixed(2)}% / ${meanDiffAbs.toFixed(4)}ms ±${meanDiffAbsMoe.toFixed(4)}ms`,
  );
  console.log(
    `  Median: ${medianDiffRel.toFixed(2)}% ±${medianDiffRelMoe.toFixed(2)}% / ${medianDiffAbs.toFixed(4)}ms ±${medianDiffAbsMoe.toFixed(4)}ms`,
  );

  console.log('\n=== CI output ===');

  console.log(`PERF_METRIC:CDP_MEAN_ABS:VALUE:${cdpFinal.mean.toFixed(4)}`);
  console.log(
    `PERF_METRIC:CDP_MEAN_ABS:RANGE:${cdpFinal.marginOfErrorMean.toFixed(4)}`,
  );

  console.log(`PERF_METRIC:CDP_MEDIAN_ABS:VALUE:${cdpFinal.median.toFixed(4)}`);
  console.log(
    `PERF_METRIC:CDP_MEDIAN_ABS:RANGE:${cdpFinal.marginOfErrorMedian.toFixed(4)}`,
  );

  console.log(`PERF_METRIC:BIDI_MEAN_ABS:VALUE:${bidiFinal.mean.toFixed(4)}`);
  console.log(
    `PERF_METRIC:BIDI_MEAN_ABS:RANGE:${bidiFinal.marginOfErrorMean.toFixed(4)}`,
  );

  console.log(
    `PERF_METRIC:BIDI_MEDIAN_ABS:VALUE:${bidiFinal.median.toFixed(4)}`,
  );
  console.log(
    `PERF_METRIC:BIDI_MEDIAN_ABS:RANGE:${bidiFinal.marginOfErrorMedian.toFixed(4)}`,
  );

  console.log(`PERF_METRIC:DIFF_MEAN_REL:VALUE:${meanDiffRel.toFixed(4)}`);
  console.log(`PERF_METRIC:DIFF_MEAN_REL:RANGE:${meanDiffRelMoe.toFixed(4)}`);

  console.log(`PERF_METRIC:DIFF_MEDIAN_REL:VALUE:${medianDiffRel.toFixed(4)}`);
  console.log(
    `PERF_METRIC:DIFF_MEDIAN_REL:RANGE:${medianDiffRelMoe.toFixed(4)}`,
  );
}

await main();

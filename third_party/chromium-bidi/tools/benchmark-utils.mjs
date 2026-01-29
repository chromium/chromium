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

const {values} = parseArgs({
  options: {
    runs: {
      type: 'string',
      default: process.env.RUNS || '2',
    },
    iterations: {
      type: 'string',
      default: process.env.ITERATIONS || '1000',
    },
  },
  strict: false,
});

export const RUNS = values.runs;
export const ITERATIONS_PER_RUN = values.iterations;
export const WARMUP_ITERATIONS = Math.max(2, 0.1 * ITERATIONS_PER_RUN);

// Critical Value (z*) for 95% Confidence Interval using the T-distribution
// with infinite degrees of freedom (which converges to the normal distribution/Z-score).
// We use 1.96 because our sample size (N=100) is > 30, making the normal approximation
// valid via the Central Limit Theorem.
// If N were smaller (< 30), we would need to calculate T_CRIT based on N-1 degrees of freedom.
export const T_CRIT_95_LARGE_N = 1.96;
export const OS = process.env.OS || 'unknown';
export const METRICS_JSON_FILE = process.env.METRICS_JSON_FILE;

export const BENCHMARK_HTML = `
<div style='font-family:Segoe UI, sans-serif; padding:20px; background:#f4f7f6;'>
  <h2>Protocol Benchmark</h2>
  <div style='display:flex; gap:15px;'>
    <div id='some-box' style='flex:1; padding:15px; background:white; border-left:5px solid #3498db;'>
      Some counter<div id='some-counter' style='font-size:24px;'>0</div><div id='some-res'>-</div>
    </div>
  </div>
</div>`;

/**
 * Calculates the Standard Error for a specific percentile (e.g., Median, P10)
 * using Rank-Based Consistency Intervals.
 *
 * Why Rank-Based?
 * Unlike the Mean, obtaining the SE for a median or percentile is non-trivial and
 * distribution-dependent if done parametrically. Rank-based methods provide a
 * "distribution-free" way to estimate the confidence interval for a quantile by
 * looking at the values at specific sorted indices (ranks) that bound the desired quantile
 * with a certain probability (95% in this case).
 *
 * The Standard Error is then approximated by taking the width of this 95% CI
 * and dividing by 2 * Z (2 * 1.96), effectively reversing the CI formula used for the mean.
 *
 * @param {number[]} sortedLatencies - Array of latencies, MUST be sorted.
 * @param {number} percentile - The percentile to estimate (0 to 1, e.g., 0.5 for Median).
 * @returns {number} The estimated Standard Error for the given percentile.
 */
export function calculateRankBasedStandardError(sortedLatencies, percentile) {
  const count = sortedLatencies.length;

  // Standard Error of the Index (Sort Order Standard Error):
  // Formula: sqrt(N * p * (1 - p))
  // This tells us the expected standard deviation of the *rank* (position) of the percentile
  // if we were to resample.
  const seIndex = Math.sqrt(count * percentile * (1 - percentile));

  // Determine the Lower and Upper bounds indices for the 95% Confidence Interval of the Rank.
  // We go +/- 1.96 standard errors from the expected rank (N * p).
  let lowerIndex = Math.floor(count * percentile - T_CRIT_95_LARGE_N * seIndex);
  let upperIndex = Math.ceil(count * percentile + T_CRIT_95_LARGE_N * seIndex);

  // Clamp indices to ensure they are within valid array bounds [0, N-1].
  lowerIndex = Math.max(0, lowerIndex);
  upperIndex = Math.min(count - 1, upperIndex);

  // Retrieve the actual latency values at these rank boundaries.
  const lowerValue = sortedLatencies[lowerIndex];
  const upperValue = sortedLatencies[upperIndex];

  // Approximate the Standard Error of the value itself.
  // Since 95% CI is roughly (Value +/- 1.96 * SE), the total width is (2 * 1.96 * SE).
  // Thus, SE ≈ Width / (2 * 1.96).
  return (upperValue - lowerValue) / (2 * T_CRIT_95_LARGE_N);
}

/**
 * Calculates a comprehensive set of statistical metrics for a benchmark run.
 *
 * Input:
 * @param {number[]} latencies - An array of raw latency measurements (e.g., in milliseconds).
 *
 * Output object contains:
 * - Central Tendency: Mean, Median
 * - Dispersion: Min, Max, StdDev, RSD (Relative Standard Deviation)
 * - Reliability: Standard Error (SE) and Margin of Error (MOE) for Mean and Percentiles.
 *
 * Why Calculate SE and MOE?
 * Raw averages are misleading without context on variance. SE and MOE allow us to state:
 * "We are 95% confident the true mean is between X and Y."
 * This is crucial for detecting regressions in CI where noise is inevitable.
 */
export function calculateStats(latencies) {
  // Sort data first, as percentiles and rank-based SE require ordered data.
  // Use numeric sort (a-b), otherwise JS default sort converts to strings!
  latencies.sort((a, b) => a - b);

  const count = latencies.length;
  const totalSum = latencies.reduce((sum, val) => sum + val, 0);

  // 1. Mean (Arithmetic Average)
  // Sensitive to outliers, but the most common metric for "throughput".
  const mean = totalSum / count;

  // Extremes
  const min = latencies[0];
  const max = latencies[count - 1];

  // 2. Percentiles (Median/P50, P10)
  // Robust against outliers.
  // Median (P50): Typical performance.
  // P10: Fast cases (best 10%).
  const median = latencies[Math.floor(count * 0.5)];
  const p10 = latencies[Math.floor(count * 0.1)];

  // 3. Variance and Standard Deviation (StdDev)
  // Variance = Average squared deviation from the Mean.
  // We use Bessel's correction (N-1) because we are working with a *sample*, not the full population.
  const variance =
    latencies.reduce((sum, val) => sum + Math.pow(val - mean, 2), 0) /
    (count - 1);

  // StdDev is in the same units as the data (ms).
  const stdDev = Math.sqrt(variance);

  // Relative Standard Deviation (RSD) / Coefficient of Variation
  // Normalized dispersion (percentage). Useful to compare stability across different tests independent of scale.
  const rsd = (stdDev / mean) * 100;

  // 4. Standard Error of the Mean (SE Mean)
  // SE = StdDev / sqrt(N)
  // Represents the unexpected variation of the *calculated mean* if we repeated the experiment 100 times.
  // As N increases, SE decreases (our estimate of the mean gets more precise).
  const standardErrorMean = stdDev / Math.sqrt(count);

  // Margin of Error (MOE) for the Mean (95% Confidence)
  // MOE = 1.96 * SE
  // The " ± X " part of the result.
  const tCrit = T_CRIT_95_LARGE_N;
  const marginOfErrorMean = tCrit * standardErrorMean;

  // CI Relative Standard Deviation (CI RSD) for Mean
  // Expresses MOE as a percentage of the Mean. "Mean is X ± 5%"
  const ciRsdMean = (marginOfErrorMean / mean) * 100;

  // 5. SE and MOE for Percentiles (Median, P10)
  // Calculated using the rank-based method defined above.

  const standardErrorMedian = calculateRankBasedStandardError(latencies, 0.5);
  const marginOfErrorMedian = tCrit * standardErrorMedian;
  const ciRsdMedian = (marginOfErrorMedian / median) * 100;

  const standardErrorP10 = calculateRankBasedStandardError(latencies, 0.1);
  const marginOfErrorP10 = tCrit * standardErrorP10;
  const ciRsdP10 = (marginOfErrorP10 / p10) * 100;

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
    p10,
    standardErrorP10,
    marginOfErrorP10,
    ciRsdP10,
  };
}

/**
 * Helper to print human-readable statistics to the console.
 * Displays Mean, Median, and P10 with their respective margins of error.
 */
export function printStats(name, final) {
  console.log(`${name}:`);
  console.log(
    `  Mean: ${final.mean.toFixed(4)}ms ±${final.marginOfErrorMean.toFixed(4)}ms (±${final.ciRsdMean.toFixed(2)}%)`,
  );
  console.log(
    `  Median: ${final.median.toFixed(4)}ms ±${final.marginOfErrorMedian.toFixed(4)}ms (±${final.ciRsdMedian.toFixed(2)}%)`,
  );
  console.log(
    `  P10: ${final.p10.toFixed(4)}ms ±${final.marginOfErrorP10.toFixed(4)}ms (±${final.ciRsdP10.toFixed(2)}%)`,
  );
}

/**
 * Calculates and prints the relative difference between two comparison points.
 * Includes the Standard Error of the Difference (SE_diff) and Margin of Error (MOE).
 *
 * @param {string} name - Name of the metric (e.g., 'Mean', 'Median').
 * @param {number} finalValue - Value from the test run (e.g., BiDi).
 * @param {number} baselineValue - Value from the baseline run (e.g., CDP).
 * @param {number} finalSe - Standard Error of the test run.
 * @param {number} baselineSe - Standard Error of the baseline run.
 */
/**
 * Calculates relative difference metrics including error propagation.
 *
 * @param {number} finalValue - Value from the test run (e.g., BiDi).
 * @param {number} baselineValue - Value from the baseline run (e.g., CDP).
 * @param {number} finalSe - Standard Error of the test run.
 * @param {number} baselineSe - Standard Error of the baseline run.
 * @returns {object} { diffAbs, diffRel, diffSe, diffMoe, diffRelMoe }
 */
function calculateDiffMetrics(finalValue, baselineValue, finalSe, baselineSe) {
  // Absolute difference (ms)
  const diffAbs = finalValue - baselineValue;

  // Relative difference (%)
  const diffRel = (diffAbs / baselineValue) * 100;

  // Standard Error of the absolute difference.
  const diffSeAbs = Math.sqrt(Math.pow(finalSe, 2) + Math.pow(baselineSe, 2));
  const diffMoeAbs = diffSeAbs * T_CRIT_95_LARGE_N;

  // Standard Error of the relative difference (Ratio Error Propagation).
  // R = A/B - 1
  // SE(R) ~= |A/B| * sqrt( (SE_A/A)^2 + (SE_B/B)^2 )
  // We use the ratio (final/baseline) for the error calculation.
  const ratio = finalValue / baselineValue - 1;
  const relErrFinal = finalSe / finalValue;
  const relErrBaseline = baselineSe / baselineValue;

  const diffSeRel =
    Math.abs(ratio) *
    Math.sqrt(Math.pow(relErrFinal, 2) + Math.pow(relErrBaseline, 2));

  // Convert SE to MOE (95% CI) and then to percentage.
  const diffRelMoe = diffSeRel * T_CRIT_95_LARGE_N * 100;

  return {
    diffAbs,
    diffRel,
    diffSeAbs,
    diffMoeAbs,
    diffRelMoe,
  };
}

/**
 * Calculates and prints the relative difference between two comparison points.
 * Includes the Standard Error of the Difference (SE_diff) and Margin of Error (MOE).
 *
 * @param {string} name - Name of the metric (e.g., 'Mean', 'Median').
 * @param {number} finalValue - Value from the test run (e.g., BiDi).
 * @param {number} baselineValue - Value from the baseline run (e.g., CDP).
 * @param {number} finalSe - Standard Error of the test run.
 * @param {number} baselineSe - Standard Error of the baseline run.
 */
function printDiff(name, finalValue, baselineValue, finalSe, baselineSe) {
  const {diffAbs, diffRel, diffMoeAbs, diffRelMoe} = calculateDiffMetrics(
    finalValue,
    baselineValue,
    finalSe,
    baselineSe,
  );

  console.log(
    `  ${name.padEnd(6)}: ${diffRel.toFixed(2)}% ±${diffRelMoe.toFixed(2)}% / ${diffAbs.toFixed(4)}ms ±${diffMoeAbs.toFixed(4)}ms`,
  );
}

/**
 * Calculates and prints the relative difference between two benchmark runs (BiDi vs Baseline).
 * Includes the Standard Error of the Difference (SE_diff) to provide a confidence interval for the regression/improvement.
 */
export function printComparison(bidiFinal, baselineFinal) {
  printDiff(
    'Mean',
    bidiFinal.mean,
    baselineFinal.mean,
    bidiFinal.standardErrorMean,
    baselineFinal.standardErrorMean,
  );
  printDiff(
    'Median',
    bidiFinal.median,
    baselineFinal.median,
    bidiFinal.standardErrorMedian,
    baselineFinal.standardErrorMedian,
  );
  printDiff(
    'P10',
    bidiFinal.p10,
    baselineFinal.p10,
    bidiFinal.standardErrorP10,
    baselineFinal.standardErrorP10,
  );
}

/**
 * Outputs structured performance metrics to a JSON file for CI consumption.
 * This function iterates over key metrics (Mean, Median, P10) and calculates the percentage difference vs baseline.
 *
 * @param {string} prefix - Prefix for the metric name (e.g., 'classic', 'bidi').
 * @param {object} final - Stats object for the test run.
 * @param {object} baseline - Stats object for the baseline run (CDP).
 * @param {string} toolName - Context name (e.g., 'selenium', 'puppeteer').
 */
export function printCiComparison(prefix, final, baseline, toolName) {
  const printMetric = (
    name,
    finalValue,
    baselineValue,
    finalSe,
    baselineSe,
  ) => {
    const {diffRel, diffRelMoe} = calculateDiffMetrics(
      finalValue,
      baselineValue,
      finalSe,
      baselineSe,
    );

    const metric = {
      // Metric name: <os>-shell:<tool>-perf-metric:<prefix>_<metric>_rel
      // e.g. linux-shell:selenium-perf-metric:bidi_mean_rel (actually `bidi_diff_rel_mean` or similar based on impl)
      name: `${OS}-shell:${toolName}-perf-metric:${prefix}_${name.toLowerCase()}_rel`,
      value: diffRel,
      range: diffRelMoe,
      unit: 'Percent',
      extra: `${OS}-shell:${toolName}-perf-metric:diff_rel`,
    };
    if (METRICS_JSON_FILE) {
      // Append as JSON line
      fs.appendFileSync(METRICS_JSON_FILE, `${JSON.stringify(metric)},\n`);
    } else {
      // Fallback to console log if no file specified
      console.log(`PERF_METRIC=${JSON.stringify(metric)}`);
    }
  };

  printMetric(
    'mean',
    final.mean,
    baseline.mean,
    final.standardErrorMean,
    baseline.standardErrorMean,
  );
  printMetric(
    'median',
    final.median,
    baseline.median,
    final.standardErrorMedian,
    baseline.standardErrorMedian,
  );
  printMetric(
    'p10',
    final.p10,
    baseline.p10,
    final.standardErrorP10,
    baseline.standardErrorP10,
  );
}

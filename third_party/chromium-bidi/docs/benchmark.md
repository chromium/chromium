# Performance Benchmarks for Chromium-BiDi

Dashboard: https://googlechromelabs.github.io/chromium-bidi/bench/index.html.

This document outlines the performance benchmarking infrastructure for chromium-bidi. Metrics are collected to better understand the protocol's overhead and stability.

## Goals

The primary objectives of the benchmarking system are:

- **Quantify Overhead:** Accurately measure the additional latency introduced by the chromium-bidi translation layer.
- **Detect Regressions:** Identify performance regressions early in the development lifecycle, ideally on every commit.
- **Analyze Stability:** Evaluate the reliability of different statistical metrics in shared CI environments.
- **Guide Optimization:** Provide data-driven insights to prioritize performance improvements.

Benchmarks run on GitHub Actions via the headless shell (to minimize latency and noise from Chrome itself) and are published automatically to the dashboard upon every PR merge.

## Puppeteer: BiDi Mode vs. CDP Mode

This benchmark compares Puppeteer running in BiDi mode against standard CDP mode. It serves as the primary metric for evaluating chromium-bidi's translation overhead.

### Methodology

Identical script evaluations (involving complex object serialization) are run in both modes. For each mode, the browser is launched **100 times** and scripts are evaluated **100 times per launch**, measuring the performance of each individual execution to ensure granular data collection.

To ensure long-term consistency and minimize noise from external dependency updates, these tests are run using a **fixed version of Puppeteer** (currently v24.36.0) against the **current chromium-bidi revision**. This allows performance changes to be attributed directly to internal logic rather than changes in the automation library.

### The Goal

Because Puppeteer in BiDi mode essentially translates commands into CDP calls, the performance difference between these two modes serves as a proxy for BiDi's overhead. The ultimate objective is to achieve a 0% performance delta, ensuring BiDi performs identically to CDP. While reaching this parity is ambitious, the goal is to minimize the overhead as much as possible.

## Selenium: Protocol Comparison

Script execution is evaluated across three different configurations:

- Raw CDP
- WebDriver Classic
- WebDriver BiDi

### Methodology

Similar to the Puppeteer benchmark, the tests are executed via chromedriver, which is the standard communication path for Selenium with Chromium. For each configuration, **10 browser launches** are performed and scripts are evaluated **100 times per launch** to gather granular performance data.

## E2E Benchmark

Metrics with the `e2e-` prefix are derived from E2E tests in the [`tests/performance`](tests/performance) folder.

Unlike the relative benchmarks above, these measure **absolute time** (e.g., for `browsingContext.captureScreenshot`), making them more sensitive to hardware fluctuations and background tasks on CI runners. They provide a baseline for the absolute latency of complex operations.

## Stability and Noise

Running benchmarks in shared CI environments like GitHub Actions introduces significant noise and jitter.

- **Linux Focus:** The Ubuntu-based runners provide the most stable data and are used as the primary source for regression detection.
- **MacOS Jitter:** Data from MacOS runners is notably noisier and is primarily used for cross-platform stability checks rather than precise overhead measurement.
- **Headless Shell:** Benchmarks utilize `headless-shell` to minimize interference from browser UI and system-level rendering overhead.

## Dashboard Data

To provide more insightful data beyond simple averages, the reporting page includes _Median_ and _P10_ values alongside the _Mean_. This assists in identifying regression trends even when outliers skew the average.

- **Mean:** The standard average. Includes every data point but is sensitive to outliers.
- **Median (P50):** Highly resistant to extreme outliers, representing typical performance.
- **P10 (10th Percentile):** Represents the **"peak efficiency"** or "ideal" scenario by filtering out the noise of system delays and random errors. This value should stay close to the theoretical minimum time required for an operation, providing a clear baseline of the protocol's peak efficiency.

## Mathematics

To ensure statistical rigor, error margins are calculated for all metrics.

### Standard Error for the Mean

The standard formula for the Standard Error (SE) of the mean is used:

`SE_mean = StdDev / sqrt(N)`

Where `StdDev` is the standard deviation and `N` is the sample size. The 95% Margin of Error (MOE) is calculated as `1.96 * SE_mean`.

### Standard Error for Percentiles (Median & P10)

Calculating the standard error for percentiles is non-trivial compared to the mean. A **distribution-free Rank-Based** method is used.

1.  **Calculate the Standard Error of the Rank (Index):**
    `SE_index = sqrt(N * p * (1 - p))` Where `p` is the percentile (e.g., 0.5 for Median, 0.1 for P10).

2.  **Determine the 95% Confidence Interval Indices:**
    `Index_lower = floor(N * p - 1.96 * SE_index)`
    `Index_upper = ceil(N * p + 1.96 * SE_index)`

3.  **Estimate the Standard Error of the Value:**
    `SE_percentile ≈ (Value[Index_upper] - Value[Index_lower]) / (2 * 1.96)`

This method estimates the width of the confidence interval around the percentile and derives the SE from that width.

### Standard Error for Relative Difference

When comparing two runs (e.g., BiDi vs. CDP), they are treated as independent samples to calculate the margin of error of their difference.

1.  **Standard Error of the Difference:**
    `SE_diff = sqrt(SE_final^2 + SE_baseline^2)`
    Where `SE_final` and `SE_baseline` are the standard errors of the respective metric (Mean, Median, or P10) being compared.

2.  **Margin of Error for the Difference:** `MOE_diff = 1.96 * SE_diff`

3.  **Relative Margin of Error (Percentage):**
    `MOE_rel% = (MOE_diff / Value_baseline) * 100`
    Where `Value_baseline` is the baseline value (e.g., the Mean of CDP) being used as a reference.

## Running Benchmarks Locally

### Preparation

Ensure you have installed the project dependencies and built the project:

```bash
npm install
npm run build
```

### Puppeteer Benchmark

To run the Puppeteer benchmark locally:

```bash
npm install puppeteer@24.36.0
npm link
npm link chromium-bidi
node tools/benchmark-puppeteer.mjs
```

You can customize the number of runs and iterations using `--runs` and `--iterations`
arguments:

```bash
node tools/benchmark-puppeteer.mjs --runs=5 --iterations=100
```

### Selenium Benchmark

To run the Selenium benchmark:

```bash
node tools/benchmark-selenium.mjs --runs=5 --iterations=100
```

### E2E Benchmark

Run the e2e tests with the `-rP` option to get the performance metrics in the output:

```bash
PYTEST_ADDOPTS="-rP" npm run e2e -- tests/performance | grep "PERF_METRIC"
```

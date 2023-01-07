// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function extractMetrics(browser, traceurl, metrics) {
  const page = await browser.newPage();
  let histograms = {};
  try {
    await page.goto(traceurl, {waitUntil: 'domcontentloaded'});
    await page.waitForFunction(() => {
      return g_timelineViewEl && g_timelineViewEl.model;
    }, {timeout: 15000});
    histograms = await page.evaluate((metricName) => {
      try {
        const histograms = new tr.v.HistogramSet();
        const metric =
            tr.metrics.MetricRegistry.findTypeInfoWithName(metricName);
        metric.constructor(histograms, g_timelineViewEl.model);

        const values = {};
        for (const h of histograms) {
          const name = h.name;
          const avg = h.average;
          const min = h.min;
          const max = h.max;
          const count = h.numValues;
          values[name] = {avg, min, max, count};
        }
        return values;
      } catch (ex) {
        return {error: `${ex}`};
      }
    }, metrics);
    await page.close();
  } catch (ex) {
    histograms.error = ex;
    await page.close();
  }
  return histograms;
}

module.exports = {
  extractMetrics,
};

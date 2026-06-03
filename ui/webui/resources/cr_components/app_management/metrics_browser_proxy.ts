// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MetricsBrowserProxy {
  recordEnumerationValue(metricName: string, value: number, enumSize: number) {
    chrome.metricsPrivate.recordEnumerationValue(metricName, value, enumSize);
  }

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxy());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;

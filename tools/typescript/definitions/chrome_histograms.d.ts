// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.histograms API.
 */

// This API is a lightweight replacement for chrome.metricsPrivate API. This API
// has no dependency on extensions and does not require the WebUIController to
// register additional Mojo interfaces.
//
// Usage: extend your WebUIController from ui::EnableMojoWebUI and set its
// `enable_chrome_histograms` constructor parameter to true.
//
// class MyWebUIController : public ui::EnableMojoWebUI,
//                           public content::WebUIController {
//  public:
//   MyWebUIController() : ui::EnableMojoWebUI(
//       /*enable_chrome_send=*/false,
//       /*enable_chrome_histograms=*/true) {}
// }
declare namespace chrome {
  export namespace histograms {

    export enum MetricTypeType {
      HISTOGRAM_LOG = 'histogram-log',
      HISTOGRAM_LINEAR = 'histogram-linear',
    }

    export interface MetricType {
      metricName: string;
      type: MetricTypeType;
      min: number;
      max: number;
      buckets: number;
    }

    export function recordUserAction(name: string): void;

    export function recordBoolean(name: string, value: boolean): void;

    export function recordPercentage(name: string, value: number): void;

    export function recordSmallCount(name: string, value: number): void;

    export function recordMediumCount(name: string, value: number): void;

    export function recordCount(name: string, value: number): void;

    export function recordTime(name: string, value: number): void;

    export function recordMediumTime(name: string, value: number): void;

    export function recordLongTime(name: string, value: number): void;

    export function recordValue(metric: MetricType, value: number): void;

    export function recordEnumerationValue(
        name: string, sample: number, enumSize: number): void;

    export function recordSparseValue(name: string, value: number): void;
  }
}

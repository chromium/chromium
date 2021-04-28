// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.metricsPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace metricsPrivate {

    export enum MetricTypeType {
      HISTOGRAM_LOG = 'histogram-log',
      HISTOGRAM_LINEAR = 'histogram-linear'
    }

    export interface MetricType {
      metricName: string;
      type: MetricTypeType;
      min: number;
      max: number;
      buckets: number;
    }

    export interface HistogramBucket {
      min: number;
      max: number;
      count: number;
    }

    export interface Histogram {
      sum: number;
      buckets: HistogramBucket[];
    }

    export function getHistogram(
        name: string, callback: (p1: Histogram) => void);
    export function getIsCrashReportingEnabled(callback: (p1: boolean) => void);
    export function getFieldTrial(name: string, callback: (p1: string) => void);
    export function getVariationParams(
        name: string, callback: (p1: Object|undefined) => void);
    export function recordPercentage(metricName: string, value: number);
    export function recordCount(metricName: string, value: number);
    export function recordSmallCount(metricName: string, value: number);
    export function recordMediumCount(metricName: string, value: number);
    export function recordTime(metricName: string, value: number);
    export function recordMediumTime(metricName: string, value: number);
    export function recordLongTime(metricName: string, value: number);
    export function recordSparseHashable(metricName: string, value: string);
    export function recordSparseValue(metricName: string, value: number);
    export function recordValue(metric: MetricType, value: number);
    export function recordBoolean(metricName: string, value: boolean);
    export function recordEnumerationValue(
        metricName: string, value: number, enumSize: number);
  }
}

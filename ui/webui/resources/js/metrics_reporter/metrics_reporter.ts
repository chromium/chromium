// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {assert} from '../assert.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';

function timeFromMojo(delta: TimeDelta): bigint {
  return delta.microseconds;
}

function timeToMojo(mark: bigint): TimeDelta {
  return {microseconds: mark};
}

/*
 * MetricsReporter: A Time Measuring Utility.
 *
 * Usages:
 *   - Use getInstance() to acquire the singleton of MetricsReporter.
 *   - Use mark(markName) to mark a timestamp.
 *     Use measure(startMark[, endMark]) to measure the duration between two
 * marks. If `endMark` is not given, current time will be used.
 *   - Use umaReportTime(metricsName, duration) to record UMA histogram.
 *   - You can call these functions from the C++ side. The marks are accessible
 * from both C++ and WebUI Javascript.
 *
 * Examples:
 *   1. Pure JS.
 *     metricsReporter.mark('StartMark');
 *     // your code to be measured ...
 *     metricsReporter.umaReportTime(
 *       'Your.Histogram', await metricsReporter.measure('StartMark'));
 *
 *   2. C++ & JS.
 *     // In C++
 *     void PageAction() {
 *       metrics_reporter_.Mark("StartMark");
 *       page->PageAction();
 *     }
 *     // In JS
 *     pageAction() {
 *       // your code to be measure ...
 *       metricsReporter.umaReportTime(
 *         metricsReporter.measure('StartMark'));
 *     }
 *
 * Caveats:
 *   1. measure() will assert if the mark is not available. You can use
 *      catch() to prevent execution from being interrupted, e.g.
 *
 *      metricsReporter.measure('StartMark').then(duration =>
 *         metricsReporter.umaReportTime('Your.Histogram', duration))
 *      .catch(() => {})
 *
 *   2. measure() will record inaccurate time if a mark is reused for
 *      overlapping measurements. To prevent this, you can:
 *
 *      a. check if a mark exists using hasLocalMark() before calling mark().
 *      b. check if a mark exists using hasMark() before calling measure().
 *      c. erase a mark using clearMark() after calling measure().
 *
 *      Alternative to b., you can use an empty catch() to ignore
 *      missing marks (due to the mark deletion by c.).
 */

export interface MetricsReporter {
  mark(name: string): void;
  measure(startMark: string, endMark?: string): Promise<bigint>;
  hasMark(name: string): Promise<boolean>;
  hasLocalMark(name: string): boolean;
  clearMark(name: string): void;
  umaReportTime(histogram: string, time: bigint): void;
}

export class MetricsReporterImpl implements MetricsReporter {
  private marks_: Map<string, bigint> = new Map();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    const callbackRouter = this.browserProxy_.getCallbackRouter();
    callbackRouter.onGetMark.addListener(
        (name: string) => ({
          markedTime:
              this.marks_.has(name) ? timeToMojo(this.marks_.get(name)!) : null,
        }));

    callbackRouter.onClearMark.addListener(
        (name: string) => this.marks_.delete(name));
  }

  static getInstance(): MetricsReporter {
    return instance || (instance = new MetricsReporterImpl());
  }

  static setInstanceForTest(newInstance: MetricsReporter) {
    instance = newInstance;
  }

  mark(name: string) {
    this.marks_.set(name, this.browserProxy_.now());
  }

  async measure(startMark: string, endMark?: string): Promise<bigint> {
    let endTime: bigint;
    if (endMark) {
      const entry = this.marks_.get(endMark);
      assert(entry, `Mark "${endMark}" does not exist locally.`);
      endTime = entry;
    } else {
      endTime = this.browserProxy_.now();
    }

    let startTime: bigint;
    if (this.marks_.has(startMark)) {
      startTime = this.marks_.get(startMark)!;
    } else {
      const remoteStartTime = await this.browserProxy_.getMark(startMark);
      assert(
          remoteStartTime.markedTime,
          `Mark "${startMark}" does not exist locally or remotely.`);
      startTime = timeFromMojo(remoteStartTime.markedTime);
    }

    return endTime - startTime;
  }

  async hasMark(name: string): Promise<boolean> {
    if (this.marks_.has(name)) {
      return true;
    }
    const remoteMark = await this.browserProxy_.getMark(name);
    return remoteMark !== null && remoteMark.markedTime !== null;
  }

  hasLocalMark(name: string): boolean {
    return this.marks_.has(name);
  }

  clearMark(name: string) {
    this.marks_.delete(name);
    this.browserProxy_.clearMark(name);
  }

  umaReportTime(histogram: string, time: bigint) {
    this.browserProxy_.umaReportTime(histogram, timeToMojo(time));
  }
}

let instance: MetricsReporter|null = null;

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeepPartial} from './util.js';

/**
 * A wrapper of `typeof chrome` because we can't use `typeof chrome` in
 * exported function parameters.
 */
type ChromeType = typeof chrome;

/**
 * Installs a mock object to replace window.chrome in a unit test.
 */
export function installMockChrome(mockChrome: DeepPartial<ChromeType>) {
  window.chrome = window.chrome || {};
  mockChrome.metricsPrivate = mockChrome.metricsPrivate || new MockMetrics();

  const chrome = window.chrome;
  const keys = Object.keys(mockChrome) as Array<keyof ChromeType>;
  for (const key of keys) {
    const value = mockChrome[key];
    const target = chrome[key] || value;
    Object.assign(target, value);
    // TS thinks the types on both sides are no compatible, hence the "any".
    chrome[key] = target as any;
  }
}

/**
 * Mocks out the chrome.fileManagerPrivate.onDirectoryChanged and getSizeStats
 * methods to be useful in unit tests.
 */
export class MockChromeFileManagerPrivateDirectoryChanged {
  /**
   * Listeners attached to listen for directory changes.
   * */
  private listeners_: Array<(event: Event) => void> = [];

  /**
   * Mocked out size stats to return when testing.
   */
  private sizeStats_:
      Record<string, chrome.fileManagerPrivate.MountPointSizeStats|undefined> =
          {};

  /**
   * Mocked out drive quota metadata to return when testing.
   */
  private driveQuotaMetadata_?: chrome.fileManagerPrivate.DriveQuotaMetadata;


  constructor() {
    window.chrome = window.chrome || {};

    window.chrome.fileManagerPrivate = window.chrome.fileManagerPrivate || {};

    /* eslint-disable-next-line @typescript-eslint/ban-ts-comment */
    // @ts-ignore: The file_manager_private.d.ts don't allow to overwrite
    // `onDirectoryChanged`.
    window.chrome.fileManagerPrivate.onDirectoryChanged =
        window.chrome.fileManagerPrivate.onDirectoryChanged || {};

    window.chrome.fileManagerPrivate.onDirectoryChanged.addListener =
        this.addListener_.bind(this);

    window.chrome.fileManagerPrivate.onDirectoryChanged.removeListener =
        this.removeListener_.bind(this);

    window.chrome.fileManagerPrivate.getSizeStats =
        this.getSizeStats_.bind(this);

    window.chrome.fileManagerPrivate.getDriveQuotaMetadata =
        this.getDriveQuotaMetadata_.bind(this);
  }

  /**
   * Store a copy of the listener to emit changes to
   */
  private addListener_(newListener: (event: Event) => void) {
    this.listeners_.push(newListener);
  }

  private removeListener_(listenerToRemove: (event: Event) => void) {
    for (let i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i] === listenerToRemove) {
        this.listeners_.splice(i, 1);
        return;
      }
    }
  }

  /**
   * Returns the stubbed out file stats for a directory change.
   * @param volumeId The underlying volumeId requesting size stats for.
   */
  private getSizeStats_(
      volumeId: string,
      callback:
          (sizeStats?: chrome.fileManagerPrivate.MountPointSizeStats) => void) {
    if (!this.sizeStats_[volumeId]) {
      callback(undefined);
      return;
    }

    callback(this.sizeStats_[volumeId]);
  }

  /**
   * Sets the size stats for the volumeId, to return when testing.
   */
  setVolumeSizeStats(
      volumeId: string,
      sizeStats?: chrome.fileManagerPrivate.MountPointSizeStats) {
    this.sizeStats_[volumeId] = sizeStats;
  }

  /**
   * Remove the sizeStats for the volumeId which can emulate getSizeStats
   * returning back undefined.
   * @param volumeId The volumeId to unset.
   */
  unsetVolumeSizeStats(volumeId: string) {
    delete this.sizeStats_[volumeId];
  }

  /**
   * Returns the stubbed out drive quota metadata for a directory change.
   */
  private getDriveQuotaMetadata_(
      _entry: Entry,
      callback:
          (sizeStats?: chrome.fileManagerPrivate.DriveQuotaMetadata) => void) {
    callback(this.driveQuotaMetadata_);
  }

  /**
   * Sets the drive quota metadata to be returned when testing.
   */
  setDriveQuotaMetadata(driveQuotaMetadata?:
                            chrome.fileManagerPrivate.DriveQuotaMetadata) {
    this.driveQuotaMetadata_ = driveQuotaMetadata;
  }

  /**
   * Set the drive quota metadata to undefined to emulate getDriveQuotaMetadata_
   * returning back undefined.
   */
  unsetDriveQuotaMetadata() {
    this.driveQuotaMetadata_ = undefined;
  }

  /**
   * Invoke all the listeners attached to the
   * chrome.fileManagerPrivate.onDirectoryChanged method.
   */
  dispatchOnDirectoryChanged() {
    const event = new Event('fake-event');
    (event as any).entry = 'fake-entry';

    for (const listener of this.listeners_) {
      listener(event);
    }
  }
}

/**
 * Mock for chrome.metricsPrivate.
 *
 * It records the method calls made to chrome.metricsPrivate.
 *
 * Typical usage:
 * const mockMetrics = new MockMetrics();
 *
 * // NOTE: installMockChrome() mocks metricsPrivate by default, which useful
 * when you don't want to check the calls to metrics.
 * installMockChrome({
 *   metricsPrivate: mockMetrics,
 * });
 *
 * // Run the code under test:
 * Then check the calls made to metrics private using either:
 * mockMetrics.apiCalls
 * mockMetrics.metricCalls
 */
export class MockMetrics {
  /**
   * Maps the API name to every call which is an array of the call
   * arguments.
   *
   */
  apiCalls: Record<string, undefined|any[]> = {};

  /**
   * Maps the metric names to every call with its arguments, similar to
   * `apiCalls` but recorded by metric instead of API method.
   *
   */
  metricCalls: Record<string, undefined|any[]> = {};

  // The API has this enum which referenced in the code.
  // Inconsistent name here because of chrome.metricsPrivate.MetricType.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  MetricTypeType: Record<string, string> = {
    'HISTOGRAM_LINEAR': 'HISTOGRAM_LINEAR',
  };

  call(apiName: string, args: any[]) {
    console.info(apiName, args);
    this.apiCalls[apiName] = this.apiCalls[apiName] || [];
    this.apiCalls[apiName]?.push(args);
    if (args.length > 0) {
      let metricName = args[0];
      // Ignore the first position because it's the metric name.
      let metricArgs = args.slice(1);
      // Some APIs uses `metricName` instead of first argument.
      if (metricName.metricName) {
        metricArgs = [metricName, ...metricArgs];
        metricName = metricName.metricName;
      }
      this.metricCalls[metricName] = this.metricCalls[metricName] || [];
      this.metricCalls[metricName]?.push(metricArgs);
    }
  }

  recordMediumCount(...args: any[]) {
    this.call('recordMediumCount', args);
  }
  recordSmallCount(...args: any[]) {
    this.call('recordSmallCount', args);
  }
  recordTime(...args: any[]) {
    this.call('recordTime', args);
  }
  recordBoolean(...args: any[]) {
    this.call('recordBoolean', args);
  }
  recordUserAction(...args: any[]) {
    this.call('recordUserAction', args);
  }
  recordValue(...args: any[]) {
    this.call('recordValue', args);
  }
  recordInterval(...args: any[]) {
    this.call('recordInterval', args);
  }
  recordEnum(...args: any[]) {
    this.call('recordEnum', args);
  }

  // To make MockMetrics compatible with chrome.metricsPrivate.
  getHistogram(_name: string): Promise<chrome.metricsPrivate.Histogram> {
    throw new Error('not implemented');
  }
  getIsCrashReportingEnabled(): Promise<boolean> {
    throw new Error('not implemented');
  }
  getFieldTrial(_name: string): Promise<string> {
    throw new Error('not implemented');
  }
  getVariationParams(_name: string): Promise<{[key: string]: string}> {
    throw new Error('not implemented');
  }
  recordPercentage(_metricName: string, _value: number): void {
    throw new Error('not implemented');
  }
  recordCount(_metricName: string, _value: number): void {
    throw new Error('not implemented');
  }
  recordMediumTime(_metricName: string, _value: number): void {
    throw new Error('not implemented');
  }
  recordLongTime(_metricName: string, _value: number): void {
    throw new Error('not implemented');
  }
  recordSparseValueWithHashMetricName(_metricName: string, _value: string):
      void {
    throw new Error('not implemented');
  }
  recordSparseValueWithPersistentHash(_metricName: string, _value: string):
      void {
    throw new Error('not implemented');
  }
  recordSparseValue(_metricName: string, _value: number): void {
    throw new Error('not implemented');
  }
  recordEnumerationValue(
      _metricName: string, _value: number, _enumSize: number): void {
    throw new Error('not implemented');
  }
}

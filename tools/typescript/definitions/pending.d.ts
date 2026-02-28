// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some DOM element properties that are available in Chrome
// but not in TypeScript lib.d.ts since they're not generally available on all
// browsers.

// The following interface are removed from TypeScript 4.4 default
// lib.dom.d.ts.
// See https://github.com/microsoft/TypeScript-DOM-lib-generator/issues/1029
// for detail.
interface DocumentOrShadowRoot {
  getSelection(): Selection|null;
}

interface HTMLElement {
  scrollIntoViewIfNeeded(): void;
}

interface HTMLDialogElement {
  close(returnValue?: string): void;
  open: boolean;
  returnValue: string;
  showModal(): void;
}

// https://developer.mozilla.org/en-US/docs/Web/API/UIEvent/sourceCapabilities
interface UIEvent extends Event {
  readonly sourceCapabilities: InputDeviceCapabilities|null;
}

// https://developer.mozilla.org/en-US/docs/Web/API/InputDeviceCapabilities
declare class InputDeviceCapabilities {
  readonly firesTouchEvents: boolean;
  readonly pointerMovementScrolls: boolean;
  constructor(param: {firesTouchEvents: boolean});
}

interface UIEventInit {
  // https://developer.mozilla.org/en-US/docs/Web/API/UIEvent/sourceCapabilities
  sourceCapabilities?: InputDeviceCapabilities|null;
}

// Barcode Detection API, this is currently only supported in Chrome on
// ChromeOS, Android or macOS.
// https://wicg.github.io/shape-detection-api/
// https://developer.mozilla.org/en-US/docs/Web/API/BarcodeDetector
declare class BarcodeDetector {
  static getSupportedFormats(): Promise<BarcodeFormat[]>;
  constructor(barcodeDetectorOptions?: BarcodeDetectorOptions);
  detect(image: ImageBitmapSource): Promise<DetectedBarcode[]>;
}

interface BarcodeDetectorOptions {
  formats?: BarcodeFormat[];
}

interface DetectedBarcode {
  boundingBox: DOMRectReadOnly;
  rawValue: string;
  format: BarcodeFormat;
  cornerPoints: ReadonlyArray<{x: number, y: number}>;
}

type BarcodeFormat =
    'aztec'|'codabar'|'code_39'|'code_93'|'code_128'|'data_matrix'|'ean_8'|
    'ean_13'|'itf'|'pdf417'|'qr_code'|'unknown'|'upc_a'|'upc_e';


// TODO(b/338624981): This should be exported in mediapipe's vision.d.ts.
/** A two-dimensional matrix. */
declare interface Matrix {
  /** The number of rows. */
  rows: number;

  /** The number of columns. */
  columns: number;

  /** The values as a flattened one-dimensional array. */
  data: number[];
}

// https://github.com/w3c/csswg-drafts/issues/9452
interface ScrollIntoViewOptions {
  container?: ScrollIntoViewContainer;
}

type ScrollIntoViewContainer = 'all'|'nearest';

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/
interface Set<T> {
  difference(other: Set<T>): Set<T>;
}

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/fromBase64
interface Uint8ArrayConstructor {
  fromBase64(string: string, options?: {
    alphabet?: string,
    lastChunkHandling?: string
  }): Uint8Array;
}

// https://developer.mozilla.org/en-US/docs/Web/API/ImageCapture/grabFrame
interface ImageCapture {
  grabFrame(): Promise<ImageBitmap>;
}

// https://developer.mozilla.org/en-US/docs/Web/API/PerformanceObserver/observe
interface PerformanceObserverInit {
  durationThreshold?: number;
}

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/DurationFormat
declare namespace Intl {
  interface DurationFormatOptions {
    localeMatcher?: 'best fit'|'lookup';
    style?: 'long'|'short'|'narrow'|'digital';
    years?: 'long'|'short'|'narrow';
    months?: 'long'|'short'|'narrow';
    weeks?: 'long'|'short'|'narrow';
    days?: 'long'|'short'|'narrow';
    hours?: 'long'|'short'|'narrow'|'numeric'|'2-digit';
    minutes?: 'long'|'short'|'narrow'|'numeric'|'2-digit';
    seconds?: 'long'|'short'|'narrow'|'numeric'|'2-digit';
    milliseconds?: 'long'|'short'|'narrow'|'numeric';
    microseconds?: 'long'|'short'|'narrow'|'numeric';
    nanoseconds?: 'long'|'short'|'narrow'|'numeric';
    fractionalDigits?: 0|1|2|3|4|5|6|7|8|9;
    numberingSystem?: string;
  }

  interface Duration {
    years?: number;
    months?: number;
    weeks?: number;
    days?: number;
    hours?: number;
    minutes?: number;
    seconds?: number;
    milliseconds?: number;
    microseconds?: number;
    nanoseconds?: number;
  }

  interface DurationFormat {
    format(duration: Duration): string;
    resolvedOptions(): DurationFormatOptions;
  }

  const DurationFormat: {
    new (locales?: string|string[], options?: DurationFormatOptions):
        DurationFormat,
    supportedLocalesOf(
        locales: string|string[], options?: DurationFormatOptions): string[],
  };
}

// See https://github.com/microsoft/TypeScript/issues/46135.
declare module '*.css' {
  const _default: CSSStyleSheet;
  export default _default;
}

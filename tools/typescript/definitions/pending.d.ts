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
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {bytesToString, getFileErrorString, str} from './translations.js';

/**
 * Tests the formatting of bytesToString.
 */
export function testBytesToString() {
  const KB = 2 ** 10;
  const MB = 2 ** 20;
  const GB = 2 ** 30;
  const TB = 2 ** 40;
  const PB = 2 ** 50;

  // Up to 1KB is displayed as 'bytes'.
  assertEquals(bytesToString(0), '0 bytes');
  assertEquals(bytesToString(10), '10 bytes');
  assertEquals(bytesToString(KB - 1), '1,023 bytes');
  assertEquals(bytesToString(KB), '1 KB');

  // Up to 1MB is displayed as a number of KBs with no precision digit.
  assertEquals(bytesToString(2 * KB), '2 KB');
  assertEquals(bytesToString(2 * KB + 1), '3 KB');
  assertEquals(bytesToString(MB - KB), '1,023 KB');
  assertEquals(bytesToString(MB - KB + 1), '1,024 KB');
  assertEquals(bytesToString(MB - 1), '1,024 KB');
  assertEquals(bytesToString(MB), '1 MB');

  // Up to 1GB is displayed as a number of MBs with <= 1 precision digit.
  assertEquals(bytesToString(2.55 * MB - 1), '2.5 MB');
  assertEquals(bytesToString(2.55 * MB), '2.6 MB');
  assertEquals(bytesToString(GB - 0.05 * MB - 1), '1,023.9 MB');
  assertEquals(bytesToString(GB - 0.05 * MB), '1,024 MB');
  assertEquals(bytesToString(GB - 1), '1,024 MB');
  assertEquals(bytesToString(GB), '1 GB');

  // Up to 1TB is displayed as a number of GBs with <= 1 precision digit.
  assertEquals(bytesToString(2.55 * GB - 1), '2.5 GB');
  assertEquals(bytesToString(2.55 * GB), '2.6 GB');
  assertEquals(bytesToString(TB - 0.05 * GB - 1), '1,023.9 GB');
  assertEquals(bytesToString(TB - 0.05 * GB), '1,024 GB');
  assertEquals(bytesToString(TB - 1), '1,024 GB');
  assertEquals(bytesToString(TB), '1 TB');

  // Up to 1PB is displayed as a number of TBs with <= 1 precision digit.
  assertEquals(bytesToString(2.55 * TB - 1), '2.5 TB');
  assertEquals(bytesToString(2.55 * TB), '2.6 TB');
  assertEquals(bytesToString(PB - 0.05 * TB - 1), '1,023.9 TB');
  assertEquals(bytesToString(PB - 0.05 * TB), '1,024 TB');
  assertEquals(bytesToString(PB - 1), '1,024 TB');
  assertEquals(bytesToString(PB), '1 PB');

  // Above 1PB is displayed as a number of PBs with <= 1 precision digit.
  assertEquals(bytesToString(2.55 * PB - 1), '2.5 PB');
  assertEquals(bytesToString(2.55 * PB), '2.6 PB');
}

/**
 * Tests the formatting of bytesToString with added precision digits.
 *
 * Useful information regarding the below assertions:
 * 647 bytes = 0.6318359375 KB.
 * bytesToString() internally uses Number.toLocaleString() which outputs at most
 * 3 precision digits in en-US.
 */
export function testBytesToStringWithAddedPrecision() {
  const KB = 2 ** 10;
  const MB = 2 ** 20;
  const GB = 2 ** 30;
  const TB = 2 ** 40;
  const PB = 2 ** 50;

  // Up to 1KB formatting unchanged.
  assertEquals(bytesToString(0, 1), '0 bytes');
  assertEquals(bytesToString(10, 2), '10 bytes');
  assertEquals(bytesToString(KB - 1, 3), '1,023 bytes');

  // Exact values, no trailing 0s regardless of precision.
  assertEquals(bytesToString(2 * KB, 1), '2 KB');
  assertEquals(bytesToString(2 * MB, 2), '2 MB');
  assertEquals(bytesToString(2 * GB, 3), '2 GB');
  assertEquals(bytesToString(2 * TB, 4), '2 TB');
  assertEquals(bytesToString(2 * PB, 5), '2 PB');

  // up to MB, initially no precision digits.
  assertEquals(bytesToString(2 * KB + 647), '3 KB');
  assertEquals(bytesToString(2 * KB + 647, 0), '3 KB');
  assertEquals(bytesToString(2 * KB + 647, 1), '2.6 KB');
  assertEquals(bytesToString(2 * KB + 647, 2), '2.63 KB');
  assertEquals(bytesToString(2 * KB + 647, 3), '2.632 KB');
  assertEquals(bytesToString(2 * KB + 647, 4), '2.632 KB');
  assertEquals(bytesToString(200 * KB + 647, 1), '200.6 KB');

  // From 1MB, up to 1 precision digit + added precision.
  assertEquals(bytesToString(2 * MB + 647 * KB), '2.6 MB');
  assertEquals(bytesToString(2 * MB + 647 * KB, 0), '2.6 MB');
  assertEquals(bytesToString(2 * MB + 647 * KB, 1), '2.63 MB');
  assertEquals(bytesToString(2 * MB + 647 * KB, 2), '2.632 MB');
  assertEquals(bytesToString(2 * MB + 647 * KB, 3), '2.632 MB');
  assertEquals(bytesToString(200 * MB + 647 * KB, 1), '200.63 MB');

  // Up to 1TB.
  assertEquals(bytesToString(2 * GB + 647 * MB), '2.6 GB');
  assertEquals(bytesToString(2 * GB + 647 * MB, 0), '2.6 GB');
  assertEquals(bytesToString(2 * GB + 647 * MB, 1), '2.63 GB');
  assertEquals(bytesToString(2 * GB + 647 * MB, 2), '2.632 GB');
  assertEquals(bytesToString(2 * GB + 647 * MB, 3), '2.632 GB');
  assertEquals(bytesToString(200 * GB + 647 * MB, 1), '200.63 GB');

  // Up to 1PB.
  assertEquals(bytesToString(2 * TB + 647 * GB), '2.6 TB');
  assertEquals(bytesToString(2 * TB + 647 * GB, 0), '2.6 TB');
  assertEquals(bytesToString(2 * TB + 647 * GB, 1), '2.63 TB');
  assertEquals(bytesToString(2 * TB + 647 * GB, 2), '2.632 TB');
  assertEquals(bytesToString(2 * TB + 647 * GB, 3), '2.632 TB');
  assertEquals(bytesToString(200 * TB + 647 * GB, 1), '200.63 TB');

  // Above 1PB.
  assertEquals(bytesToString(2 * PB + 647 * TB), '2.6 PB');
  assertEquals(bytesToString(2 * PB + 647 * TB, 0), '2.6 PB');
  assertEquals(bytesToString(2 * PB + 647 * TB, 1), '2.63 PB');
  assertEquals(bytesToString(2 * PB + 647 * TB, 2), '2.632 PB');
  assertEquals(bytesToString(2 * PB + 647 * TB, 3), '2.632 PB');
  assertEquals(bytesToString(200 * PB + 647 * TB, 1), '200.63 PB');
}

/**
 * Tests the getFileErrorString helper for undefined, null, or empty
 * string error name input, which should output an i18n FILE_ERROR_GENERIC
 * error name.
 *
 * Also tests pre-defined error names ('NotFoundError' and 'PathExistsError'
 * here), which should output their associated i18n error names.
 */
export function testGetFileErrorString() {
  let i18nErrorName;

  i18nErrorName = getFileErrorString(undefined);
  assertEquals(i18nErrorName, str('FILE_ERROR_GENERIC'));

  i18nErrorName = getFileErrorString(null);
  assertEquals(i18nErrorName, str('FILE_ERROR_GENERIC'));

  i18nErrorName = getFileErrorString('');
  assertEquals(i18nErrorName, str('FILE_ERROR_GENERIC'));

  i18nErrorName = getFileErrorString('NotFoundError');
  assertEquals(i18nErrorName, str('FILE_ERROR_NOT_FOUND'));

  i18nErrorName = getFileErrorString('PathExistsError');
  assertEquals(i18nErrorName, str('FILE_ERROR_PATH_EXISTS'));
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {TimeTicks} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

/** Converts a String16 to a JavaScript String. */
export function decodeString16(str: String16|null): string {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

/** Converts a JavaScript String to a String16. */
export function mojoString16(str: string): String16 {
  const array = new Array(str.length);
  for (let i = 0; i < str.length; ++i) {
    array[i] = str.charCodeAt(i);
  }
  return {data: array};
}

/**
 * Converts a time ticks in milliseconds to TimeTicks.
 * @param timeTicks time ticks in milliseconds
 */
export function mojoTimeTicks(timeTicks: number): TimeTicks {
  return {internalValue: BigInt(Math.floor(timeTicks * 1000))};
}

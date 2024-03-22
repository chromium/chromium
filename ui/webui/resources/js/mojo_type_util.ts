// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

// Convert a javascript string into a Mojo String16.
export function stringToMojoString16(str: string): String16 {
  const arr: number[] = [];
  for (let i = 0; i < str.length; i++) {
    arr.push(str.charCodeAt(i));
  }
  return {data: arr};
}

// Convert a Mojo String16 into a javascript string.
export function mojoString16ToString(str16: String16): string {
  // Taken from chunk size used in goog.crypt.byteArrayToBinaryString in Closure
  // Library. The value is equal to 2^13.
  const CHUNK_SIZE = 8192;

  if (str16.data.length < CHUNK_SIZE) {
    return String.fromCharCode(...str16.data);
  }

  // Convert the array to a string in chunks, to avoid passing too many
  // arguments to String.fromCharCode() at once, which can exceed the max call
  // stack size (c.f. crbug.com/1509792).
  let str = '';
  for (let i = 0; i < str16.data.length; i += CHUNK_SIZE) {
    const chunk = str16.data.slice(i, i + CHUNK_SIZE);
    str += String.fromCharCode(...chunk);
  }
  return str;
}

// Note: This does not do any validation of the URL string.
export function stringToMojoUrl(s: string): Url {
  return {url: s};
}

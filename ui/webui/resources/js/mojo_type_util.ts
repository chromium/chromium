// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

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
  return String.fromCharCode(...str16.data);
}

// Note: This does not do any validation of the URL string.
export function stringToMojoUrl(s: string): Url {
  return {url: s};
}

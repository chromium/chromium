// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

export function stringToMojoString16(s: string): String16 {
  return {data: Array.from(s, c => c.charCodeAt(0))};
}

export function mojoString16ToString(str16: String16): string {
  return str16.data.map((ch: number) => String.fromCodePoint(ch)).join('');
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

// Note: This does not do any validation of the URL string.
export function stringToMojoUrl(s: string): Url {
  return {url: s};
}

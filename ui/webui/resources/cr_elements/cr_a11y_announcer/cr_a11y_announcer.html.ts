// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from '//resources/js/static_types.js';

export function getTemplate() {
  return getTrustedHTML`
<div id="messages" role="alert" aria-live="polite" aria-relevant="additions">
</div>`;
}

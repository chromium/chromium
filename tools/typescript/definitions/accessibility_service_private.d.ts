// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.accessibilityServicePrivate API
 * Generated from:
 * chrome/common/extensions/api/accessibility_service_private.idl run
 * `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/accessibility_service_private.idl -g
 * ts_definitions` to regenerate.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace accessibilityServicePrivate {

      export function speakSelectedText(): Promise<void>;

      export const clipboardCopyInActiveGoogleDoc:
          ChromeEvent<(url: string) => void>;

    }
  }
}
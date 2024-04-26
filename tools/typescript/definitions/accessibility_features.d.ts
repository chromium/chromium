// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.accessibilityFeatures API. */
// TODO(crbug.com/40179454): Auto-generate this file
// from chrome/common/extensions/api/accessibility_features.json.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace accessibilityFeatures {

      export interface ChromeSettingParams {
        name?: string;
      }

      export interface ChromeSettingsResponse {
        value: boolean;
      }

      export interface ChromeSetting {
        get(details: ChromeSettingParams): Promise<ChromeSettingsResponse>;
        onChange: ChromeEvent<(details: ChromeSettingsResponse) => void>;
      }

      export const spokenFeedback: ChromeSetting;

      export const selectToSpeak: ChromeSetting;

      export const switchAccess: ChromeSetting;

    }
  }
}

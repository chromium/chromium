// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.offscreen API
 * Generated from: extensions/common/api/offscreen.idl
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/offscreen.idl -g ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace offscreen {

    export enum Reason {
      TESTING = 'TESTING',
      AUDIO_PLAYBACK = 'AUDIO_PLAYBACK',
      IFRAME_SCRIPTING = 'IFRAME_SCRIPTING',
      DOM_SCRAPING = 'DOM_SCRAPING',
      BLOBS = 'BLOBS',
      DOM_PARSER = 'DOM_PARSER',
      USER_MEDIA = 'USER_MEDIA',
      DISPLAY_MEDIA = 'DISPLAY_MEDIA',
      WEB_RTC = 'WEB_RTC',
      CLIPBOARD = 'CLIPBOARD',
      LOCAL_STORAGE = 'LOCAL_STORAGE',
      WORKERS = 'WORKERS',
      BATTERY_STATUS = 'BATTERY_STATUS',
      MATCH_MEDIA = 'MATCH_MEDIA',
      GEOLOCATION = 'GEOLOCATION',
    }

    export interface CreateParameters {
      reasons: Reason[];
      url: string;
      justification: string;
    }

    export function createDocument(parameters: CreateParameters): Promise<void>;

    export function closeDocument(): Promise<void>;

    export function hasDocument(): Promise<boolean>;

  }
}


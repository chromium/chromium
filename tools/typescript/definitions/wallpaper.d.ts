// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.wallpaper API
 * Generated from: chrome/common/extensions/api/wallpaper.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/wallpaper.json -g ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace wallpaper {

    export enum WallpaperLayout {
      STRETCH = 'STRETCH',
      CENTER = 'CENTER',
      CENTER_CROPPED = 'CENTER_CROPPED',
    }

    export function setWallpaper(details: {
      data?: ArrayBuffer,
      url?: string, layout: WallpaperLayout, filename: string,
      thumbnail?: boolean,
    }): Promise<ArrayBuffer|undefined>;
  }
}

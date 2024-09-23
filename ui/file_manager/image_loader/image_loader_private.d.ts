// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.imageLoaderPrivate API
 * Generated from: chrome/common/extensions/api/image_loader_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/image_loader_private.idl -g ts_definitions` to
 * regenerate.
 */



declare namespace chrome {
  export namespace imageLoaderPrivate {

    export function getDriveThumbnail(
        url: string, cropToSquare: boolean,
        callback: (thumbnailDataUrl: string) => void): void;

    export function getPdfThumbnail(
        url: string, width: number, height: number,
        callback: (thumbnailDataUrl: string) => void): void;

    export function getArcDocumentsProviderThumbnail(
        url: string, widthHint: number, heightHint: number,
        callback: (thumbnailDataUrl: string) => void): void;
  }
}

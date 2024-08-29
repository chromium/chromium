// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for messages passed between the SW (service
 * worker) and OD (offscreen document).
 *
 * The FooBarPrivateApi types capture the arguments for imageLoaderPrivate
 * calls made by the SW on behalf of the OD. These definitions are manually
 * written based on the auto-generated image_loader_private.d.ts.
 */

export interface GetDriveThumbnailPrivateApi {
  apiMethod: 'getDriveThumbnail';
  params: {
    url: string,
    cropToSquare: boolean,
  };
}

export interface GetPdfThumbnailPrivateApi {
  apiMethod: 'getPdfThumbnail';
  params: {
    url: string,
    width: number,
    height: number,
  };
}

export interface GetArcDocumentsProviderThumbnailPrivateApi {
  apiMethod: 'getArcDocumentsProviderThumbnail';
  params: {
    url: string,
    widthHint: number,
    heightHint: number,
  };
}

export type PrivateApi = GetDriveThumbnailPrivateApi|GetPdfThumbnailPrivateApi|
    GetArcDocumentsProviderThumbnailPrivateApi;

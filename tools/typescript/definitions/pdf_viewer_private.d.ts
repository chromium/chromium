// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.pdfViewerPrivate API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace pdfViewerPrivate {
      // Keep in sync with the values for enum `SaveRequestType` in
      // `pdf/mojom/pdf.mojom` and
      // `chrome/common/extensions/api/pdf_viewer_private.idl`.
      export enum SaveRequestType {
        ANNOTATION = 'ANNOTATION',
        ORIGINAL = 'ORIGINAL',
        EDITED = 'EDITED',
        SEARCHIFIED = 'SEARCHIFIED',
      }

      // <if expr="enable_pdf_save_to_drive">
      // Keep in sync with the values for enum `SaveToDriveErrorType` in
      // chrome/common/extensions/api/pdf_viewer_private.idl and
      // `PdfSaveToDriveErrorType` in
      // tools/metrics/histograms/metadata/pdf/enums.xml.
      export enum SaveToDriveErrorType {
        NO_ERROR = 'NO_ERROR',
        UNKNOWN_ERROR = 'UNKNOWN_ERROR',
        QUOTA_EXCEEDED = 'QUOTA_EXCEEDED',
        OFFLINE = 'OFFLINE',
        OAUTH_ERROR = 'OAUTH_ERROR',
        ACCOUNT_CHOOSER_CANCELED = 'ACCOUNT_CHOOSER_CANCELED',
        PARENT_FOLDER_SELECTION_FAILED = 'PARENT_FOLDER_SELECTION_FAILED',
      }

      // Keep in sync with the values for enum `SaveToDriveStatus` in
      // chrome/common/extensions/api/pdf_viewer_private.idl and
      // `PdfSaveToDriveStatus` in
      // tools/metrics/histograms/metadata/pdf/enums.xml.
      export enum SaveToDriveStatus {
        NOT_STARTED = 'NOT_STARTED',
        INITIATED = 'INITATED',
        ACCOUNT_CHOOSER_SHOWN = 'ACCOUNT_CHOOSER_SHOWN',
        ACCOUNT_SELECTED = 'ACCOUNT_SELECTED',
        ACCOUNT_ADD_SELECTED = 'ACCOUNT_ADD_SELECTED',
        ACCOUNT_ADDED = 'ACCOUNT_ADDED',
        FETCH_OAUTH = 'FETCH_OAUTH',
        FETCH_PARENT_FOLDER = 'FETCH_PARENT_FOLDER',
        UPLOAD_STARTED = 'UPLOAD_STARTED',
        UPLOAD_IN_PROGRESS = 'UPLOAD_IN_PROGRESS',
        UPLOAD_COMPLETED = 'UPLOAD_COMPLETED',
        UPLOAD_RETRIED = 'UPLOAD_RETRIED',
        UPLOAD_FAILED = 'UPLOAD_FAILED',
      }

      export interface SaveToDriveProgress {
        status: SaveToDriveStatus;
        errorType: SaveToDriveErrorType;
        driveItemId?: string;
        fileSizeBytes?: number;
        uploadedBytes?: number;
        fileMetadata?: string;
        fileName?: string;
        parentFolderName?: string;
        accountEmail?: string;
        accountIsManaged?: boolean;
      }
      // </if>

      // `mimeType` and `responseHeaders` are unused fields, but they are
      // necessary to be able to cast to chrome.mimeHandlerPrivate.StreamInfo.
      // TODO(crbug.com/40268279): Remove `mimeType` and `responseHeaders` after
      // PDF viewer no longer uses chrome.mimeHandlerPrivate.
      export interface StreamInfo {
        mimeType: string;
        originalUrl: string;
        streamUrl: string;
        tabId: number;
        responseHeaders: Record<string, string>;
        embedded: boolean;
      }

      export interface PdfPluginAttributes {
        backgroundColor: number;
        allowJavascript: boolean;
      }

      export function getStreamInfo(callback: (info: StreamInfo) => void): void;
      export function isAllowedLocalFileAccess(
          url: string, callback: (isAllowed: boolean) => void): void;
      // <if expr="enable_pdf_save_to_drive">
      export function saveToDrive(saveRequestType?: SaveRequestType): void;
      // </if>
      export function setPdfDocumentTitle(title: string): void;
      export function setPdfPluginAttributes(attributes: PdfPluginAttributes):
          void;
      export const onSave: ChromeEvent<(url: string) => void>;
      // <if expr="enable_pdf_save_to_drive">
      export const onSaveToDriveProgress:
          ChromeEvent<(url: string, progress: SaveToDriveProgress) => void>;
      // </if>
      export const onShouldUpdateViewport: ChromeEvent<(url: string) => void>;
    }
  }
}

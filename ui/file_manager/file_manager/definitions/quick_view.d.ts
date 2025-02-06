// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview: This file describes the interfaces shared between the Files
 * app UI and the JS code run from unstrusted_resources (as part of the
 * QuickView).
 * @see `//ash/webui/file_manager/untrusted_resources/` for its usage.
 */

/*
 * File preview content. Sent from <files-quick-view> to a sandboxed page, as
 * part of UntrustedPreviewData. |data| field holds a Blob/File, or a content
 * URL. |dataType| specifies 'blob' or 'url' accordingly (other values are
 * interpreted as no preview data).
 */
declare interface FilePreviewContent {
  dataType: string;
  data?: Blob|string|null;
}

/*
 * Parameters gathered to set the Quick View dialog properties.
 */
declare interface QuickViewParams {
  type: string;
  subtype: string;
  filePath: string;
  hasTask: boolean;
  canDelete: boolean;
  sourceContent?: FilePreviewContent|null;
  videoPoster?: FilePreviewContent|null;
  audioArtwork?: FilePreviewContent|null;
  autoplay?: boolean|null;
  browsable?: boolean|null;
}

/**
 * Preview data that we send from the trusted context (Files app) to
 * the untrusted context. |type| specifies the type of file to preview ('audio',
 * 'image', etc...).
 */
declare interface UntrustedPreviewData {
  type: string;
  sourceContent: FilePreviewContent;
}

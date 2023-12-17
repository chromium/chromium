// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExifEntry, ExifTag} from './exif_constants.js';

export interface ImageTransformation {
  scaleX: number;
  scaleY: number;
  rotate90: number;
}

export type MetadataKey = keyof MetadataItem;

/**
 * Metadata of a file.
 */
export class MetadataItem {
  /**
   * Size of the file. -1 for directory.
   */
  size?: number;

  modificationTime?: Date;

  modificationTimeError?: Error;

  modificationByMeTime?: Date;

  /**
   * Thumbnail URL obtained from external provider.
   */
  thumbnailUrl?: string;

  /**
   * Cropped thumbnail URL obtained from external provider.
   */
  croppedThumbnailUrl?: string;

  croppedThumbnailUrlError?: Error;

  thumbnailUrlError?: Error;

  imageWidth?: number;

  imageHeight?: number;

  imageRotation?: number;

  /**
   * Thumbnail obtained from content provider.
   */
  contentThumbnailUrl?: string;

  contentThumbnailUrlError?: Error;

  /**
   * Thumbnail transformation obtained from content provider.
   */
  contentThumbnailTransform?: ImageTransformation;

  contentThumbnailTransformError?: Error;

  /**
   * Image transformation obtained from content provider.
   */
  contentImageTransform?: ImageTransformation;

  contentImageTransformError?: Error;

  /**
   * Whether the entry is pinned for ensuring it is available offline.
   */
  pinned?: boolean;

  /**
   * Whether the entry is cached locally.
   */
  present?: boolean;

  presentError?: Error;

  /**
   * Whether the entry is hosted document of google drive.
   */
  hosted?: boolean;

  /**
   * Whether the entry is modified locally and not synched yet.
   */
  dirty?: boolean;

  /**
   * Whether the entry is present or hosted;
   */
  availableOffline?: boolean;

  availableWhenMetered?: boolean;

  customIconUrl?: string;

  customIconUrlError?: Error;

  contentMimeType?: string;

  /**
   * Whether the entry is shared explicitly with me.
   */
  sharedWithMe?: boolean;

  /**
   * Whether the entry is shared publicly.
   */
  shared?: boolean;

  /**
   * URL for open a file in browser tab.
   */
  externalFileUrl?: string;

  mediaAlbum?: string;

  mediaArtist?: string;

  /**
   * Audio or video duration in seconds.
   */
  mediaDuration?: number;

  mediaGenre?: string;

  mediaTitle?: string;

  mediaTrack?: string;

  mediaYearRecorded?: string;

  /**
   * Mime type obtained by content provider based on URL.
   * TODO(hirono)?: Remove the mediaMimeType.
   */
  mediaMimeType?: string;

  /**
   * "Image File Directory" obtained from EXIF header.
   * TODO(b/289003444): Add a type for this field once we move the type
   * definition for Ifd into metadata.
   */
  ifd?: object;

  ifdError?: Error;

  exifLittleEndian?: boolean;

  canCopy?: boolean;

  canDelete?: boolean;

  canRename?: boolean;

  canAddChildren?: boolean;

  canShare?: boolean;

  canPin?: boolean;

  alternateUrl?: string;

  isMachineRoot?: boolean;

  isArbitrarySyncFolder?: boolean;

  isExternalMedia?: boolean;

  /**
   * Whether the entry is under any DataLeakPrevention policy.
   */
  isDlpRestricted?: boolean;

  /**
   * Source URL that can be used to check DataLeakPrevention policy.
   */
  sourceUrl?: string;

  /**
   * Only applicable in file picker dialogs.
   * Whether the entry is blocked by DataLeakPrevention policy from being
   * uploaded to/opened by a specific destination, defined by the caller of
   * the dialog.
   */
  isRestrictedForDestination?: boolean;

  /**
   * Status indicating the current syncing behaviour for this item.
   */
  syncStatus?: string;

  /**
   * Represents some ongoing operation with this item. E.g., pasting, syncing.
   * Note: currently, this is exclusively used for Drive syncing.
   */
  progress?: number;

  /**
   * If true, the item is a shortcut. Typically refers to a shortcut in Drive,
   * but used to surface a shortcut badge on the file item.
   */
  shortcut?: boolean;

  /**
   * Time in milliseconds since the epoch when the file last received a
   * "completed" sync status.
   */
  syncCompletedTime?: number;
}

export interface Id3v2Frame {
  // Three to four character identifier for the type of frame.
  name: string;
  headerSize: number;
  // Size excludes the header.
  size: number;
  // Only set for v2.3 and v2.4.
  flags?: number;

  // Various optional fields set depending on the type of frame.
  encoding?: number;
  value?: string;

  description?: string;

  format?: string;
  mime?: string;
  pictureType?: number;
  imageUrl?: string;
}

export class ParserMetadata {
  imageTransform?: ImageTransformation;
  thumbnailTransform?: ImageTransformation;
  thumbnailURL?: string;
  littleEndian?: boolean;
  ifd?: {
    image?: Record<ExifTag, ExifEntry>,
    thumbnail?: Record<ExifTag, ExifEntry>,
    exif?: Record<ExifTag, ExifEntry>,
    gps?: Record<ExifTag, ExifEntry>,
  };
  fileSize?: number;
  height?: number;
  width?: number;
  duration?: number;
  mimeType?: string;

  title?: string;
  artist?: string;
  album?: string;
  description?: Array<{key: string, value: string}>;

  mpegBrand?: string;

  id3v2?: {
    majorVersion: number,
    minorVersion: number,
    flags: number,
    size: number,
    frames: {[frameName: string]: Id3v2Frame},
  };
}

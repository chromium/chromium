// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *  scaleX: number,
 *  scaleY: number,
 *  rotate90: number
 * }}
 */
let ImageTransformation;

/**
 * Metadata of a file.
 * @unrestricted to allow access for properties via '.' or '[]', useful for
 * computed properties.
 */
class MetadataItem {
  constructor() {
    /**
     * Size of the file. -1 for directory.
     * @public {number|undefined}
     */
    this.size;

    /**
     * @public {!Date|undefined}
     */
    this.modificationTime;

    /**
     * @public {Error|undefined}
     */
    this.modificationTimeError;

    /**
     * @public {!Date|undefined}
     */
    this.modificationByMeTime;

    /**
     * Thumbnail URL obtained from external provider.
     * @public {string|undefined}
     */
    this.thumbnailUrl;

    /**
     * Cropped thumbnail URL obtained from external provider.
     * @public {string|undefined}
     */
    this.croppedThumbnailUrl;

    /**
     * @public {Error|undefined}
     */
    this.croppedThumbnailUrlError;

    /**
     * @public {Error|undefined}
     */
    this.thumbnailUrlError;

    /**
     * @public {number|undefined}
     */
    this.imageWidth;

    /**
     * @public {number|undefined}
     */
    this.imageHeight;

    /**
     * @public {number|undefined}
     */
    this.imageRotation;

    /**
     * Thumbnail obtained from content provider.
     * @public {string|undefined}
     */
    this.contentThumbnailUrl;

    /**
     * @public {Error|undefined}
     */
    this.contentThumbnailUrlError;

    /**
     * Thumbnail transformation obtained from content provider.
     * @public {!ImageTransformation|undefined}
     */
    this.contentThumbnailTransform;

    /**
     * @public {Error|undefined}
     */
    this.contentThumbnailTransformError;

    /**
     * Image transformation obtained from content provider.
     * @public {!ImageTransformation|undefined}
     */
    this.contentImageTransform;

    /**
     * @public {Error|undefined}
     */
    this.contentImageTransformError;

    /**
     * Whether the entry is pinned for ensuring it is available offline.
     * @public {boolean|undefined}
     */
    this.pinned;

    /**
     * Whether the entry is cached locally.
     * @public {boolean|undefined}
     */
    this.present;

    /**
     * @public {Error|undefined}
     */
    this.presentError;

    /**
     * Whether the entry is hosted document of google drive.
     * @public {boolean|undefined}
     */
    this.hosted;

    /**
     * Whether the entry is modified locally and not synched yet.
     * @public {boolean|undefined}
     */
    this.dirty;

    /**
     * Whether the entry is present or hosted;
     * @public {boolean|undefined}
     */
    this.availableOffline;

    /**
     * @public {boolean|undefined}
     */
    this.availableWhenMetered;

    /**
     * @public {string|undefined}
     */
    this.customIconUrl;

    /**
     * @public {Error|undefined}
     */
    this.customIconUrlError;

    /**
     * @public {string|undefined}
     */
    this.contentMimeType;

    /**
     * Whether the entry is shared explicitly with me.
     * @public {boolean|undefined}
     */
    this.sharedWithMe;

    /**
     * Whether the entry is shared publicly.
     * @public {boolean|undefined}
     */
    this.shared;

    /**
     * URL for open a file in browser tab.
     * @public {string|undefined}
     */
    this.externalFileUrl;

    /**
     * @public {string|undefined}
     */
    this.mediaAlbum;

    /**
     * @public {string|undefined}
     */
    this.mediaArtist;

    /**
     * Audio or video duration in seconds.
     * @public {number|undefined}
     */
    this.mediaDuration;

    /**
     * @public {string|undefined}
     */
    this.mediaGenre;

    /**
     * @public {string|undefined}
     */
    this.mediaTitle;

    /**
     * @public {string|undefined}
     */
    this.mediaTrack;

    /**
     * @public {string|undefined}
     */
    this.mediaYearRecorded;

    /**
     * Mime type obtained by content provider based on URL.
     * TODO(hirono): Remove the mediaMimeType.
     * @public {string|undefined}
     */
    this.mediaMimeType;

    /**
     * "Image File Directory" obtained from EXIF header.
     * @public {!Object|undefined}
     */
    this.ifd;

    /**
     * @public {boolean|undefined}
     */
    this.exifLittleEndian;

    /**
     * @public {boolean|undefined}
     */
    this.canCopy;

    /**
     * @public {boolean|undefined}
     */
    this.canDelete;

    /**
     * @public {boolean|undefined}
     */
    this.canRename;

    /**
     * @public {boolean|undefined}
     */
    this.canAddChildren;

    /**
     * @public {boolean|undefined}
     */
    this.canShare;

    /**
     * @public {string|undefined}
     */
    this.alternateUrl;

    /**
     * @public {boolean|undefined}
     */
    this.isMachineRoot;

    /**
     * @public {boolean|undefined}
     */
    this.isArbitrarySyncFolder;

    /**
     * @public {boolean|undefined}
     */
    this.isExternalMedia;
  }
}

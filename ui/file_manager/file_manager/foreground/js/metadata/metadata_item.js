// Copyright 2015 The Chromium Authors
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
export class MetadataItem {
  constructor() {
    /**
     * Size of the file. -1 for directory.
     * @type {number|undefined}
     */
    this.size;

    /** @type {!Date|undefined} */
    this.modificationTime;

    /** @type {Error|undefined} */
    this.modificationTimeError;

    /** @type {!Date|undefined} */
    this.modificationByMeTime;

    /**
     * Thumbnail URL obtained from external provider.
     * @type {string|undefined}
     */
    this.thumbnailUrl;

    /**
     * Cropped thumbnail URL obtained from external provider.
     * @type {string|undefined}
     */
    this.croppedThumbnailUrl;

    /** @type {Error|undefined} */
    this.croppedThumbnailUrlError;

    /** @type {Error|undefined} */
    this.thumbnailUrlError;

    /** @type {number|undefined} */
    this.imageWidth;

    /** @type {number|undefined} */
    this.imageHeight;

    /** @type {number|undefined} */
    this.imageRotation;

    /**
     * Thumbnail obtained from content provider.
     * @type {string|undefined}
     */
    this.contentThumbnailUrl;

    /** @type {Error|undefined} */
    this.contentThumbnailUrlError;

    /**
     * Thumbnail transformation obtained from content provider.
     * @type {!ImageTransformation|undefined}
     */
    this.contentThumbnailTransform;

    /** @type {Error|undefined} */
    this.contentThumbnailTransformError;

    /**
     * Image transformation obtained from content provider.
     * @type {!ImageTransformation|undefined}
     */
    this.contentImageTransform;

    /** @type {Error|undefined} */
    this.contentImageTransformError;

    /**
     * Whether the entry is pinned for ensuring it is available offline.
     * @type {boolean|undefined}
     */
    this.pinned;

    /**
     * Whether the entry is cached locally.
     * @type {boolean|undefined}
     */
    this.present;

    /** @type {Error|undefined} */
    this.presentError;

    /**
     * Whether the entry is hosted document of google drive.
     * @type {boolean|undefined}
     */
    this.hosted;

    /**
     * Whether the entry is modified locally and not synched yet.
     * @type {boolean|undefined}
     */
    this.dirty;

    /**
     * Whether the entry is present or hosted;
     * @type {boolean|undefined}
     */
    this.availableOffline;

    /** @type {boolean|undefined} */
    this.availableWhenMetered;

    /** @type {string|undefined} */
    this.customIconUrl;

    /** @type {Error|undefined} */
    this.customIconUrlError;

    /** @type {string|undefined} */
    this.contentMimeType;

    /**
     * Whether the entry is shared explicitly with me.
     * @type {boolean|undefined}
     */
    this.sharedWithMe;

    /**
     * Whether the entry is shared publicly.
     * @type {boolean|undefined}
     */
    this.shared;

    /**
     * URL for open a file in browser tab.
     * @type {string|undefined}
     */
    this.externalFileUrl;

    /** @type {string|undefined} */
    this.mediaAlbum;

    /** @type {string|undefined} */
    this.mediaArtist;

    /**
     * Audio or video duration in seconds.
     * @type {number|undefined}
     */
    this.mediaDuration;

    /** @type {string|undefined} */
    this.mediaGenre;

    /** @type {string|undefined} */
    this.mediaTitle;

    /** @type {string|undefined} */
    this.mediaTrack;

    /** @type {string|undefined} */
    this.mediaYearRecorded;

    /**
     * Mime type obtained by content provider based on URL.
     * TODO(hirono): Remove the mediaMimeType.
     * @type {string|undefined}
     */
    this.mediaMimeType;

    /**
     * "Image File Directory" obtained from EXIF header.
     * @type {!Object|undefined}
     */
    this.ifd;

    /** @type {boolean|undefined} */
    this.exifLittleEndian;

    /** @type {boolean|undefined} */
    this.canCopy;

    /** @type {boolean|undefined} */
    this.canDelete;

    /** @type {boolean|undefined} */
    this.canRename;

    /** @type {boolean|undefined} */
    this.canAddChildren;

    /** @type {boolean|undefined} */
    this.canShare;

    /** @type {string|undefined} */
    this.alternateUrl;

    /** @type {boolean|undefined} */
    this.isMachineRoot;

    /** @type {boolean|undefined} */
    this.isArbitrarySyncFolder;

    /** @type {boolean|undefined} */
    this.isExternalMedia;

    /**
     * Whether the entry is under any DataLeakPrevention policy.
     * @type {boolean|undefined}
     */
    this.isDlpRestricted;

    /**
     * Source URL that can be used to check DataLeakPrevention policy.
     * @type {string|undefined}
     */
    this.sourceUrl;

    /**
     * Only applicable in file picker dialogs.
     * Whether the entry is blocked by DataLeakPrevention policy from being
     * uploaded to/opened by a specific destination, defined by the caller of
     * the dialog.
     * @type {boolean|undefined}
     */
    this.isRestrictedForDestination;

    /**
     * Status indicating the current syncing behaviour for this item.
     * @type {string|undefined}
     */
    this.syncStatus;
  }
}

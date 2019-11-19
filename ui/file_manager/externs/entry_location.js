// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 * @interface
 */
class EntryLocation {
  constructor() {
    /**
     * Volume information.
     * @type {VolumeInfo}
     */
    this.volumeInfo;

    /**
     * Root type.
     * @type {VolumeManagerCommon.RootType}
     */
    this.rootType;

    /**
     * Whether the entry is root entry or not.
     * @type {boolean}
     */
    this.isRootEntry;

    /**
     * Whether the location obtained from the fake entry corresponds to special
     * searches.
     * @type {boolean}
     */
    this.isSpecialSearchRoot;

    /**
     * Whether the location is under Google Drive or a special search root which
     * represents a special search from Google Drive.
     * @type {boolean}
     */
    this.isDriveBased;

    /**
     * Whether the entry is read only or not.
     * @type {boolean}
     */
    this.isReadOnly;

    /**
     * Whether the entry should be displayed with a fixed name instead of
     * individual entry's name. (e.g. "Downloads" is a fixed name)
     * @type {boolean}
     */
    this.hasFixedLabel;
  }
}

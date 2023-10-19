// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Exif} from '../foreground/js/metadata/exif_constants.js';

/**
 * Type definition of exif entry.
 * @typedef {{id:!Exif.Tag, format:number, componentCount:number,
 *     value:(undefined|*)}}
 */
export let ExifEntry;

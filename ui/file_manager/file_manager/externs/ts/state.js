// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../files_app_entry_interfaces.js';

/**
 * The data for each individual file/entry.
 * @typedef {{
 *   entry: (Entry|FilesAppEntry),
 * }}
 */
export let FileData;

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 * @typedef {{
 *   name: string,
 *   label: string,
 *   key: string,
 * }}
 */
export let PathComponent;

/**
 * The status of a property, for properties that have their state updated via
 * asynchronous steps.
 * @enum {string}
 */
export const PropStatus = {
  STARTED: 'STARTED',

  // Finished:
  SUCCESS: 'SUCCESS',
  ERROR: 'ERROR',
};

/**
 * The current directory.
 * The directory is only effectively active when the `status` is SUCCESS.
 * @typedef {{
 *   status: !PropStatus,
 *   key: string,
 *   pathComponents: !Array<PathComponent>,
 * }}
 */
export let CurrentDirectory;

/**
 * Files app's state.
 * @typedef {{
 *   allEntries: !Object<string, FileData>,
 *   currentDirectory: (CurrentDirectory|undefined),
 * }}
 */
export let State;

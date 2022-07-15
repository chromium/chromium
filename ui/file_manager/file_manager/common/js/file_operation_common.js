// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {util} from './util.js';

export class FileOperationProgressEvent extends Event {
  /** @param {string} eventName */
  constructor(eventName) {
    super(eventName);

    /** @type {FileOperationProgressEvent.EventType} */
    this.reason;

    /** @type {(FileOperationError|undefined)} */
    this.error;

    /** @public {string} */
    this.taskId;

    /** @public {?Array<!Entry>} */
    this.entries;

    /** @public {?Object} */
    this.status;

    /** @public {number} */
    this.totalBytes;

    /** @public {number} */
    this.processedBytes;

    /** @public {?Array<!FilesAppEntry>} */
    this.trashedEntries;
  }
}

/**
 * Types of events emitted by the EventRouter.
 * @enum {string}
 */
FileOperationProgressEvent.EventType = {
  BEGIN: 'BEGIN',
  CANCELED: 'CANCELED',
  ERROR: 'ERROR',
  PROGRESS: 'PROGRESS',
  SUCCESS: 'SUCCESS',
};

/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 */
export class FileOperationError {
  /**
   * @param {util.FileOperationErrorType} code Error type.
   * @param {string|Entry|DOMError} data Additional data.
   */
  constructor(code, data) {
    this.code = code;
    this.data = data;
  }
}

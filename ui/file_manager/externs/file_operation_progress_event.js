// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class FileOperationProgressEvent extends Event {
  constructor() {
    /** @type {fileOperationUtil.EventRouter.EventType} */
    this.reason;

    /** @type {(fileOperationUtil.Error|undefined)} */
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
  }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {util} from '../../common/js/util.js';
import {xfm} from '../../common/js/xfm.js';

/**
 * TaskHistory object keeps track of the history of task executions.
 * This is responsible for keeping the history in persistent xfm.storage, too.
 */
export class TaskHistory extends EventTarget {
  constructor() {
    super();

    /**
     * The recent history of task executions. Key is task ID and value is time
     * stamp of the latest execution of the task.
     * @type {!Object<string, number>}
     */
    this.lastExecutedTime_ = {};

    xfm.storage.onChanged.addListener(this.onLocalStorageChanged_.bind(this));
    this.load_();
  }

  /**
   * Records the timing of task execution.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
   */
  recordTaskExecuted(descriptor) {
    const taskId = util.makeTaskID(descriptor);
    this.lastExecutedTime_[taskId] = Date.now();
    this.truncate_();
    this.save_();
  }

  /**
   * Gets the time stamp of last execution of given task. If the record is not
   * found, returns 0.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
   * @return {number}
   */
  getLastExecutedTime(descriptor) {
    const taskId = util.makeTaskID(descriptor);
    return this.lastExecutedTime_[taskId] ? this.lastExecutedTime_[taskId] : 0;
  }

  /**
   * Loads current history from local xfm.storage.
   * @private
   */
  load_() {
    xfm.storage.local.get(TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME, value => {
      this.lastExecutedTime_ =
          value[TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME] || {};
    });
  }

  /**
   * Saves current history to local xfm.storage.
   * @private
   */
  save_() {
    const objectToSave = {};
    objectToSave[TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME] =
        this.lastExecutedTime_;
    xfm.storage.local.set(objectToSave);
  }

  /**
   * Handles change event on xfm.storage to update current history.
   * @param {!Object<string, !StorageChange>} changes
   * @param {string} areaName
   * @private
   */
  onLocalStorageChanged_(changes, areaName) {
    if (areaName != 'local') {
      return;
    }

    for (const key in changes) {
      if (key == TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME) {
        this.lastExecutedTime_ = changes[key].newValue;
        dispatchSimpleEvent(this, TaskHistory.EventType.UPDATE);
      }
    }
  }

  /**
   * Trancates current history so that the size of history does not exceed
   * STORAGE_KEY_LAST_EXECUTED_TIME.
   * @private
   */
  truncate_() {
    const keys = Object.keys(this.lastExecutedTime_);
    if (keys.length <= TaskHistory.LAST_EXECUTED_TIME_HISTORY_MAX) {
      return;
    }

    let items = [];
    for (let i = 0; i < keys.length; i++) {
      items.push({id: keys[i], timestamp: this.lastExecutedTime_[keys[i]]});
    }

    items.sort((a, b) => b.timestamp - a.timestamp);
    items = items.slice(0, TaskHistory.LAST_EXECUTED_TIME_HISTORY_MAX);

    const newObject = {};
    for (let i = 0; i < items.length; i++) {
      newObject[items[i].id] = items[i].timestamp;
    }

    this.lastExecutedTime_ = newObject;
  }
}

/**
 * @enum {string}
 */
TaskHistory.EventType = {
  UPDATE: 'update',
};

/**
 * This key is used to store the history in chrome's local xfm.storage.
 * @const {string}
 */
TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME = 'task-last-executed-time';

/**
 * @const {number}
 */
TaskHistory.LAST_EXECUTED_TIME_HISTORY_MAX = 100;

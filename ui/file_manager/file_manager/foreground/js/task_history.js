// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {storage} from '../../common/js/storage.js';
import {makeTaskID} from '../../common/js/util.js';

/**
 * TaskHistory object keeps track of the history of task executions. Recent
 * history is stored in local storage.
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

    storage.onChanged.addListener(this.onLocalStorageChanged_.bind(this));
    this.load_();
  }

  /**
   * Records the timing of task execution.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
   */
  recordTaskExecuted(descriptor) {
    const taskId = makeTaskID(descriptor);
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
    const taskId = makeTaskID(descriptor);
    // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable to
    // type 'number'.
    return this.lastExecutedTime_[taskId] ? this.lastExecutedTime_[taskId] : 0;
  }

  /**
   * Loads the current history from local storage.
   * @private
   */
  load_() {
    storage.local.get(TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME, value => {
      this.lastExecutedTime_ =
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'string' can't be used to index type
          // 'Object'.
          value[TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME] || {};
    });
  }

  /**
   * Saves the current history to local storage.
   * @private
   */
  save_() {
    const objectToSave = {};
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    objectToSave[TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME] =
        this.lastExecutedTime_;
    storage.local.set(objectToSave);
  }

  /**
   * Handles local storage change event to update the current history.
   * @param {!Object<string,
   *     !import("../../common/js/storage.js").ChangedValue>} changes
   * @param {string} areaName
   * @private
   */
  onLocalStorageChanged_(changes, areaName) {
    if (areaName !== 'local') {
      return;
    }

    for (const key in changes) {
      if (key == TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME) {
        this.lastExecutedTime_ = changes[key]?.newValue;
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
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      items.push({id: keys[i], timestamp: this.lastExecutedTime_[keys[i]]});
    }

    items.sort((a, b) => b.timestamp - a.timestamp);
    items = items.slice(0, TaskHistory.LAST_EXECUTED_TIME_HISTORY_MAX);

    const newObject = {};
    for (let i = 0; i < items.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      newObject[items[i].id] = items[i].timestamp;
    }

    // @ts-ignore: error TS2322: Type '{}' is not assignable to type '{ [x:
    // string]: number; }'.
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
 * Key used to store the task history in local storage.
 * @const @type {string}
 */
TaskHistory.STORAGE_KEY_LAST_EXECUTED_TIME = 'task-last-executed-time';

/**
 * @const @type {number}
 */
TaskHistory.LAST_EXECUTED_TIME_HISTORY_MAX = 100;

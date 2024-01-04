// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {type ChangedValues, storage} from '../../common/js/storage.js';
import {makeTaskID} from '../../common/js/util.js';

export enum EventType {
  UPDATE = 'update',
}

/** Key used to store the task history in local storage. */
const STORAGE_KEY_LAST_EXECUTED_TIME = 'task-last-executed-time';
const LAST_EXECUTED_TIME_HISTORY_MAX = 100;

/**
 * TaskHistory object keeps track of the history of task executions. Recent
 * history is stored in local storage.
 */
export class TaskHistory extends EventTarget {
  /**
   * The recent history of task executions. Key is task ID and value is time
   * stamp of the latest execution of the task.
   */
  private lastExecutedTime_: Record<string, number> = {};

  constructor() {
    super();

    storage.onChanged.addListener(this.onLocalStorageChanged_.bind(this));
    this.load_();
  }

  /** Records the timing of task execution. */
  recordTaskExecuted(descriptor: chrome.fileManagerPrivate.FileTaskDescriptor) {
    const taskId = makeTaskID(descriptor);
    this.lastExecutedTime_[taskId] = Date.now();
    this.truncate_();
    this.save_();
  }

  /**
   * Gets the time stamp of last execution of given task. If the record is not
   * found, returns 0.
   */
  getLastExecutedTime(descriptor: chrome.fileManagerPrivate.FileTaskDescriptor):
      number {
    const taskId = makeTaskID(descriptor);
    return this.lastExecutedTime_[taskId] ?? 0;
  }

  /** Loads the current history from local storage. */
  private load_() {
    storage.local.get(
        STORAGE_KEY_LAST_EXECUTED_TIME, (value: Record<string, any>) => {
          this.lastExecutedTime_ = value[STORAGE_KEY_LAST_EXECUTED_TIME] ?? {};
        });
  }

  /** Saves the current history to local storage. */
  private save_() {
    storage.local.set(
        {[STORAGE_KEY_LAST_EXECUTED_TIME]: this.lastExecutedTime_});
  }

  /** Handles local storage change event to update the current history. */
  private onLocalStorageChanged_(changes: ChangedValues, areaName: string) {
    if (areaName !== 'local') {
      return;
    }

    for (const key in changes) {
      if (key === STORAGE_KEY_LAST_EXECUTED_TIME) {
        this.lastExecutedTime_ = changes[key]?.newValue;
        dispatchSimpleEvent(this, EventType.UPDATE);
      }
    }
  }

  /**
   * Truncates current history so that the size of history does not exceed
   * STORAGE_KEY_LAST_EXECUTED_TIME.
   */
  private truncate_() {
    const keys = Object.keys(this.lastExecutedTime_);
    if (keys.length <= LAST_EXECUTED_TIME_HISTORY_MAX) {
      return;
    }

    interface Item {
      id: string;
      timestamp: number;
    }

    let items: Item[] = [];
    for (const key of keys) {
      items.push({id: key, timestamp: this.lastExecutedTime_[key]!});
    }

    items.sort((a, b) => b.timestamp - a.timestamp);
    items = items.slice(0, LAST_EXECUTED_TIME_HISTORY_MAX);

    const newObject: Record<string, number> = {};
    for (const item of items) {
      newObject[item.id] = item.timestamp;
    }

    this.lastExecutedTime_ = newObject;
  }
}

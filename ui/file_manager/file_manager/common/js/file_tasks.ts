// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import type {TaskHistory} from '../../foreground/js/task_history.js';
import type {FileData} from '../../state/state.js';

import {getIcon} from './file_type.js';
import {str} from './translations.js';
import {LEGACY_FILES_EXTENSION_ID, SWA_APP_ID, SWA_FILES_APP_URL, toFilesAppURL} from './url_constants.js';

export interface AnnotatedTask extends chrome.fileManagerPrivate.FileTask {
  iconType: string;
}

/**
 * The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID, just the
 * sub-string compatible with the extension/legacy e.g.: "view-pdf".
 */
export function parseActionId(actionId: string): string {
  const swaUrl = SWA_FILES_APP_URL.toString() + '?';
  return actionId.replace(swaUrl, '');
}

/** Returns whether the provided appId corresponds Files app's. */
export function isFilesAppId(appId: string): boolean {
  return appId === LEGACY_FILES_EXTENSION_ID || appId === SWA_APP_ID;
}

/** The task descriptor of 'Install Linux package'. */
export const INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR = {
  appId: LEGACY_FILES_EXTENSION_ID,
  taskType: 'app',
  actionId: 'install-linux-package',
} as const;

/**
 * Gets the default task from tasks. In case there is no such task (i.e. all
 * tasks are generic file handlers), then return null.
 */
export function getDefaultTask(
    tasks: AnnotatedTask[],
    policyDefaultHandlerStatus:
        chrome.fileManagerPrivate.PolicyDefaultHandlerStatus|undefined,
    taskHistory: TaskHistory): AnnotatedTask|null {
  const INCORRECT_ASSIGNMENT =
      chrome.fileManagerPrivate.PolicyDefaultHandlerStatus.INCORRECT_ASSIGNMENT;
  const DEFAULT_HANDLER_ASSIGNED_BY_POLICY =
      chrome.fileManagerPrivate.PolicyDefaultHandlerStatus
          .DEFAULT_HANDLER_ASSIGNED_BY_POLICY;

  // If policy assignment is incorrect, then no default should be set.
  if (policyDefaultHandlerStatus &&
      policyDefaultHandlerStatus === INCORRECT_ASSIGNMENT) {
    return null;
  }

  // 1. Default app set for MIME or file extension by user, or built-in app.
  for (const task of tasks) {
    if (task.isDefault) {
      return task;
    }
  }

  // If policy assignment is marked as correct, then by this moment we
  // should've already found the default.
  console.assert(
      !(policyDefaultHandlerStatus &&
        policyDefaultHandlerStatus === DEFAULT_HANDLER_ASSIGNED_BY_POLICY));

  const nonGenericTasks = tasks.filter(t => !t.isGenericFileHandler);
  if (nonGenericTasks.length === 0) {
    return null;
  }

  // 2. Most recently executed or sole non-generic task.
  const latest = nonGenericTasks[0]!;
  if (nonGenericTasks.length === 1 ||
      taskHistory.getLastExecutedTime(latest.descriptor)) {
    return latest;
  }

  return null;
}

/**
 * Annotates tasks returned from the API.
 * @param tasks Input tasks from the API.
 * @param entries List of entries for the tasks.
 */
export function annotateTasks(
    tasks: chrome.fileManagerPrivate.FileTask[],
    entries: Array<Entry|FilesAppEntry>|FileData[]): AnnotatedTask[] {
  const result: AnnotatedTask[] = [];
  for (const task of tasks) {
    const {appId, taskType, actionId} = task.descriptor;
    const parsedActionId = parseActionId(actionId);

    // Tweak images, titles of internal tasks.
    const annotateTask: AnnotatedTask = {...task, iconType: ''};
    if (isFilesAppId(appId) && (taskType === 'app' || taskType === 'web')) {
      if (parsedActionId === 'mount-archive') {
        annotateTask.iconType = 'archive';
        annotateTask.title = str('MOUNT_ARCHIVE');
      } else if (parsedActionId === 'open-hosted-generic') {
        if (entries.length > 1) {
          annotateTask.iconType = 'generic';
        } else {  // Use specific icon.
          annotateTask.iconType = getIcon(entries[0]!);
        }
        annotateTask.title = str('TASK_OPEN');
      } else if (parsedActionId === 'open-hosted-gdoc') {
        annotateTask.iconType = 'gdoc';
        annotateTask.title = str('TASK_OPEN_GDOC');
      } else if (parsedActionId === 'open-hosted-gsheet') {
        annotateTask.iconType = 'gsheet';
        annotateTask.title = str('TASK_OPEN_GSHEET');
      } else if (parsedActionId === 'open-hosted-gslides') {
        annotateTask.iconType = 'gslides';
        annotateTask.title = str('TASK_OPEN_GSLIDES');
      } else if (parsedActionId === 'open-web-drive-office-word') {
        annotateTask.iconType = 'gdoc';
      } else if (parsedActionId === 'open-web-drive-office-excel') {
        annotateTask.iconType = 'gsheet';
      } else if (parsedActionId === 'upload-office-to-drive') {
        annotateTask.iconType = 'generic';
        annotateTask.title = 'Upload to Drive';
      } else if (parsedActionId === 'open-web-drive-office-powerpoint') {
        annotateTask.iconType = 'gslides';
      } else if (parsedActionId === 'open-in-office') {
        annotateTask.iconUrl =
            toFilesAppURL('foreground/images/files/ui/ms365.svg').toString();
      } else if (parsedActionId === 'install-linux-package') {
        annotateTask.iconType = 'crostini';
        annotateTask.title = str('TASK_INSTALL_LINUX_PACKAGE');
      } else if (parsedActionId === 'import-crostini-image') {
        annotateTask.iconType = 'tini';
        annotateTask.title = str('TASK_IMPORT_CROSTINI_IMAGE');
      } else if (parsedActionId === 'view-pdf') {
        annotateTask.iconType = 'pdf';
        annotateTask.title = str('TASK_VIEW');
      } else if (parsedActionId === 'view-in-browser') {
        annotateTask.iconType = 'generic';
        annotateTask.title = str('TASK_VIEW');
      } else if (parsedActionId === 'open-encrypted') {
        annotateTask.iconType = 'generic';
        annotateTask.title = str('TASK_OPEN_GDRIVE');
      } else if (parsedActionId === 'install-isolated-web-app') {
        annotateTask.iconType = 'removable';
      }
    }
    if (!annotateTask.iconType && taskType === 'web-intent') {
      annotateTask.iconType = 'generic';
    }

    result.push(annotateTask);
  }

  return result;
}

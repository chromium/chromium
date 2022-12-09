// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFileTasks} from '../../common/js/api.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {getNativeEntry} from '../../common/js/entry_utils.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {FileData, PropStatus} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {annotateTasks, getDefaultTask, INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR} from '../../foreground/js/file_tasks.js';
import {ActionsProducerGen} from '../../lib/actions_producer.js';
import {keyedKeepFirst} from '../../lib/concurrency_models.js';
import {ActionType} from '../actions.js';
import {ChangeFileTasksAction} from '../actions/current_directory.js';
import {getStore} from '../store.js';

/**
 * Linux package installation is currently only supported for a single file
 * which is inside the Linux container, or in a shareable volume.
 * TODO(timloh): Instead of filtering these out, we probably should show a
 * dialog with an error message, similar to when attempting to run Crostini
 * tasks with non-Crostini entries.
 */
function allowCrostiniTask(filesData: FileData[]) {
  if (filesData.length !== 1) {
    return false;
  }
  const fileData = filesData[0]!;
  const rootType = (fileData.entry as FakeEntry).rootType;
  if (rootType !== VolumeManagerCommon.RootType.CROSTINI) {
    return false;
  }
  const crostini = window.fileManager.crostini;
  return crostini.canSharePath(
      constants.DEFAULT_CROSTINI_VM, (fileData.entry as Entry),
      /*persiste=*/ false);
}

function emptyAction(status: PropStatus): ChangeFileTasksAction {
  return {
    type: ActionType.CHANGE_FILE_TASKS,
    payload: {
      tasks: [],
      policyDefaultHandlerStatus: undefined,
      defaultTask: undefined,
      status,
    },
  };
}

export async function*
    fetchFileTasksInternal(filesData: FileData[]):
        ActionsProducerGen<ChangeFileTasksAction> {
  // Filters out the non-native entries.
  filesData = filesData.filter(getNativeEntry);
  const state = getStore().getState();
  const currentRootType = state.currentDirectory?.rootType;
  const dialogType = window.fileManager.dialogType;
  const shouldDisableTasks = (
      // File Picker/Save As doesn't show the "Open" button.
      dialogType !== DialogType.FULL_PAGE ||
      // The list of available tasks should not be available to trashed items.
      currentRootType === VolumeManagerCommon.RootType.TRASH ||
      filesData.length === 0);
  if (shouldDisableTasks) {
    yield emptyAction(PropStatus.SUCCESS);
    return;
  }
  const selectionHandler = window.fileManager.selectionHandler;
  const selection = selectionHandler.selection;
  await selection.computeAdditional(window.fileManager.metadataModel);
  yield;
  try {
    const resultingTasks = await getFileTasks(filesData.map(fd => fd.entry));
    if (!resultingTasks || !resultingTasks.tasks) {
      return;
    }
    yield;
    if (filesData.length === 0 || resultingTasks.tasks.length === 0) {
      yield emptyAction(PropStatus.SUCCESS);
      return;
    }
    if (!allowCrostiniTask(filesData)) {
      resultingTasks.tasks = resultingTasks.tasks.filter(
          (task: chrome.fileManagerPrivate.FileTask) => !util.descriptorEqual(
              task.descriptor, INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR));
    }
    const tasks = annotateTasks(resultingTasks.tasks, filesData);
    resultingTasks.tasks = tasks;
    // TODO: Migrate TaskHistory to the store.
    const taskHistory = window.fileManager.taskController.taskHistory;
    const defaultTask =
        getDefaultTask(
            tasks, resultingTasks.policyDefaultHandlerStatus, taskHistory) ??
        undefined;
    yield {
      type: ActionType.CHANGE_FILE_TASKS,
      payload: {
        tasks,
        policyDefaultHandlerStatus: resultingTasks.policyDefaultHandlerStatus,
        defaultTask: defaultTask,
        status: PropStatus.SUCCESS,
      },
    };
  } catch (error) {
    yield emptyAction(PropStatus.ERROR);
  }
}

/** Generates key based on each FileKey (entry.toURL()). */
function getSelectionKey(filesData: FileData[]): string {
  return filesData.map(f => f?.entry.toURL()).join('|');
}

export const fetchFileTasks =
    keyedKeepFirst(fetchFileTasksInternal, getSelectionKey);

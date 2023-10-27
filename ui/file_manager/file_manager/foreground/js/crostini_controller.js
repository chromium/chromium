// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {isNewDirectoryTreeEnabled} from '../../common/js/flags.js';
import {str, strf} from '../../common/js/translations.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {crostiniPlaceHolderKey} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';
import {FilesToast} from '../elements/files_toast.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {CommandHandler} from './file_manager_commands.js';
import {NavigationModelFakeItem, NavigationModelItemType} from './navigation_list_model.js';
import {DirectoryTree} from './ui/directory_tree.js';

/**
 * CrostiniController handles the foreground UI relating to crostini.
 */
export class CrostiniController {
  /**
   * @param {!Crostini} crostini Crostini background object.
   * @param {!DirectoryModel} directoryModel DirectoryModel.
   * @param {!DirectoryTree} directoryTree DirectoryTree.
   * @param {boolean} disabled Whether the Crostini item should be disabled.
   *     Defaults to false.
   */
  constructor(crostini, directoryModel, directoryTree, disabled = false) {
    /** @private @const */
    this.crostini_ = crostini;

    /** @private @const */
    this.directoryModel_ = directoryModel;

    /** @private @const */
    this.directoryTree_ = directoryTree;

    /** @private */
    this.entrySharedWithCrostini_ = false;

    /** @private */
    this.entrySharedWithPluginVm_ = false;

    /**
     * @private @const @type {boolean}
     */
    this.disabled_ = disabled;
  }

  /**
   * Refresh the Linux files item at startup and when crostini enabled changes.
   */
  async redraw() {
    // Setup Linux files fake root.
    let crostiniNavigationModelItem;
    if (this.crostini_.isEnabled(constants.DEFAULT_CROSTINI_VM)) {
      const crostiniEntry = new FakeEntryImpl(
          str('LINUX_FILES_ROOT_LABEL'), VolumeManagerCommon.RootType.CROSTINI);
      crostiniNavigationModelItem = new NavigationModelFakeItem(
          str('LINUX_FILES_ROOT_LABEL'), NavigationModelItemType.CROSTINI,
          crostiniEntry);
      crostiniNavigationModelItem.disabled = this.disabled_;
      getStore().dispatch(addUiEntry({entry: crostiniEntry}));
    } else {
      crostiniNavigationModelItem = null;
      getStore().dispatch(removeUiEntry({key: crostiniPlaceHolderKey}));
    }
    if (!isNewDirectoryTreeEnabled()) {
      // @ts-ignore: error TS2322: Type 'NavigationModelFakeItem | null' is not
      // assignable to type 'NavigationModelFakeItem'.
      this.directoryTree_.dataModel.linuxFilesItem =
          crostiniNavigationModelItem;
      // Redraw the tree to ensure 'Linux files' is added/removed.
      this.directoryTree_.redraw(false);
    }
  }

  /**
   * Load the list of shared paths and show a toast if this is the first time
   * that FilesApp is loaded since login.
   *
   * @param {boolean} maybeShowToast if true, show toast if this is the first
   *     time FilesApp is opened since login.
   * @param {!FilesToast} filesToast
   */
  async loadSharedPaths(maybeShowToast, filesToast) {
    let showToast = maybeShowToast;
    // @ts-ignore: error TS7006: Parameter 'vmName' implicitly has an 'any'
    // type.
    const getSharedPaths = async (vmName) => {
      if (!this.crostini_.isEnabled(vmName)) {
        return 0;
      }

      return new Promise(resolve => {
        chrome.fileManagerPrivate.getCrostiniSharedPaths(
            maybeShowToast, vmName, (entries, firstForSession) => {
              showToast = showToast && firstForSession;
              for (const entry of entries) {
                this.crostini_.registerSharedPath(vmName, assert(entry));
              }
              resolve(entries.length);
            });
      });
    };

    // @ts-ignore: error TS7006: Parameter 'umaItem' implicitly has an 'any'
    // type.
    const toast = (count, msgSingle, msgPlural, action, subPage, umaItem) => {
      if (!showToast || count == 0) {
        return;
      }
      filesToast.show(count == 1 ? str(msgSingle) : strf(msgPlural, count), {
        text: str(action),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage(subPage);
          CommandHandler.recordMenuItemSelected(umaItem);
        },
      });
    };

    const [crostiniShareCount, pluginVmShareCount, bruschettaVmShareCount] =
        await Promise.all([
          getSharedPaths(constants.DEFAULT_CROSTINI_VM),
          getSharedPaths(constants.PLUGIN_VM),
          getSharedPaths(constants.DEFAULT_BRUSCHETTA_VM),
        ]);

    // Toasts are queued and shown one-at-a-time if multiple apply.
    // TODO(b/260521400): Or at least, they will once this bug is fixed.
    toast(
        crostiniShareCount, 'FOLDER_SHARED_WITH_CROSTINI',
        'FOLDER_SHARED_WITH_CROSTINI_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'crostini/sharedPaths',
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST_STARTUP);
    toast(
        pluginVmShareCount, 'FOLDER_SHARED_WITH_PLUGIN_VM',
        'FOLDER_SHARED_WITH_PLUGIN_VM_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'app-management/pluginVm/sharedPaths',
        CommandHandler.MenuCommandsForUMA
            .MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP);
    toast(
        bruschettaVmShareCount, 'FOLDER_SHARED_WITH_BRUSCHETTA',
        'FOLDER_SHARED_WITH_BRUSCHETTA_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'bruschetta/sharedPaths',
        CommandHandler.MenuCommandsForUMA
            .MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP);
  }
}

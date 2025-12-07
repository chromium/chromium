// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import type {Crostini} from '../../background/js/crostini.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str, strf} from '../../common/js/translations.js';
import {RootType} from '../../common/js/volume_manager_types.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {crostiniPlaceHolderKey} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';
import type {FilesToast} from '../elements/files_toast.js';

import {MenuCommandsForUma, recordMenuItemSelected} from './command_handler.js';
import {DEFAULT_BRUSCHETTA_VM, DEFAULT_CROSTINI_VM, PLUGIN_VM} from './constants.js';
/**
 * CrostiniController handles the foreground UI relating to crostini.
 */
export class CrostiniController {
  /**
   * @param crostini_ Crostini background object.
   */
  constructor(private readonly crostini_: Crostini) {}

  /**
   * Refreshes the Linux files item at startup and when crostini enabled
   * changes.
   */
  async redraw() {
    const store = getStore();
    // Setup Linux files fake root.
    if (this.crostini_.isEnabled(DEFAULT_CROSTINI_VM)) {
      const crostiniEntry =
          new FakeEntryImpl(str('LINUX_FILES_ROOT_LABEL'), RootType.CROSTINI);
      store.dispatch(addUiEntry(crostiniEntry));
    } else {
      store.dispatch(removeUiEntry(crostiniPlaceHolderKey));
    }
  }

  /**
   * Load the list of shared paths and show a toast if this is the first time
   * that FilesApp is loaded since login.
   *
   * @param maybeShowToast if true, show toast if this is the first
   *     time FilesApp is opened since login.
   */
  async loadSharedPaths(maybeShowToast: boolean, filesToast: FilesToast) {
    let showToast = maybeShowToast;
    const getSharedPaths = async(vmName: string): Promise<number> => {
      if (!this.crostini_.isEnabled(vmName)) {
        return 0;
      }

      return new Promise(resolve => {
        chrome.fileManagerPrivate.getCrostiniSharedPaths(
            maybeShowToast, vmName, ({entries, firstForSession}) => {
              showToast = showToast && firstForSession;
              for (const entry of entries) {
                this.crostini_.registerSharedPath(vmName, entry);
              }
              resolve(entries.length);
            });
      });
    };

    const toast =
        (count: number, msgSingle: string, msgPlural: string, action: string,
         subPage: string, umaItem: MenuCommandsForUma) => {
          if (!showToast || count === 0) {
            return;
          }
          filesToast.show(
              count === 1 ? str(msgSingle) : strf(msgPlural, count), {
                text: str(action),
                callback: () => {
                  chrome.fileManagerPrivate.openSettingsSubpage(subPage);
                  recordMenuItemSelected(umaItem);
                },
              });
        };

    const [crostiniShareCount, pluginVmShareCount, bruschettaVmShareCount] =
        await Promise.all([
          getSharedPaths(DEFAULT_CROSTINI_VM),
          getSharedPaths(PLUGIN_VM),
          getSharedPaths(DEFAULT_BRUSCHETTA_VM),
        ]);

    // Toasts are queued and shown one-at-a-time if multiple apply.
    // TODO(b/260521400): Or at least, they will once this bug is fixed.
    toast(
        crostiniShareCount, 'FOLDER_SHARED_WITH_CROSTINI',
        'FOLDER_SHARED_WITH_CROSTINI_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'crostini/sharedPaths',
        MenuCommandsForUma.MANAGE_LINUX_SHARING_TOAST_STARTUP);
    toast(
        pluginVmShareCount, 'FOLDER_SHARED_WITH_PLUGIN_VM',
        'FOLDER_SHARED_WITH_PLUGIN_VM_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'app-management/pluginVm/sharedPaths',
        MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP);
    toast(
        bruschettaVmShareCount, 'FOLDER_SHARED_WITH_BRUSCHETTA',
        'FOLDER_SHARED_WITH_BRUSCHETTA_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'bruschetta/sharedPaths',
        MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP);
  }
}

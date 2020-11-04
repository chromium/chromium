// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * CrostiniController handles the foreground UI relating to crostini.
 */
class CrostiniController {
  /**
   * @param {!Crostini} crostini Crostini background object.
   * @param {!FilesMessage} filesMessage FilesMessage.
   * @param {!DirectoryModel} directoryModel DirectoryModel.
   * @param {!DirectoryTree} directoryTree DirectoryTree.
   */
  constructor(crostini, filesMessage, directoryModel, directoryTree) {
    /** @private @const */
    this.crostini_ = crostini;

    /** @private @const */
    this.filesMessage_ = filesMessage;

    /** @private @const */
    this.directoryModel_ = directoryModel;

    /** @private @const */
    this.directoryTree_ = directoryTree;

    /** @private */
    this.entrySharedWithCrostini_ = false;

    /** @private */
    this.entrySharedWithPluginVm_ = false;

    directoryModel.addEventListener(
        'directory-changed', () => this.maybeShowSharedMessage());
  }

  /**
   * Refresh the Linux files item at startup and when crostini enabled changes.
   */
  async redraw() {
    // Setup Linux files fake root.
    this.directoryTree_.dataModel.linuxFilesItem =
        this.crostini_.isEnabled(constants.DEFAULT_CROSTINI_VM) ?
        new NavigationModelFakeItem(
            str('LINUX_FILES_ROOT_LABEL'), NavigationModelItemType.CROSTINI,
            new FakeEntryImpl(
                str('LINUX_FILES_ROOT_LABEL'),
                VolumeManagerCommon.RootType.CROSTINI)) :
        null;
    // Redraw the tree to ensure 'Linux files' is added/removed.
    this.directoryTree_.redraw(false);
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

    const toast = (count, msgSingle, msgPlural, action, subPage, umaItem) => {
      if (!showToast || count == 0) {
        return;
      }
      filesToast.show(count == 1 ? str(msgSingle) : strf(msgPlural, count), {
        text: str(action),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage(subPage);
          CommandHandler.recordMenuItemSelected(umaItem);
        }
      });
    };

    const [crostiniShareCount, pluginVmShareCount] = await Promise.all([
      getSharedPaths(constants.DEFAULT_CROSTINI_VM),
      getSharedPaths(constants.PLUGIN_VM)
    ]);

    toast(
        crostiniShareCount, 'FOLDER_SHARED_WITH_CROSTINI',
        'FOLDER_SHARED_WITH_CROSTINI_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'crostini/sharedPaths',
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST_STARTUP);
    // TODO(crbug.com/949356): UX to provide guidance for what to do
    // when we have shared paths with both Linux and Plugin VM.
    toast(
        pluginVmShareCount, 'FOLDER_SHARED_WITH_PLUGIN_VM',
        'FOLDER_SHARED_WITH_PLUGIN_VM_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'app-management/pluginVm/sharedPaths',
        CommandHandler.MenuCommandsForUMA
            .MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP);
  }

  maybeShowSharedMessage() {
    const entry =
        /** @type {Entry} */ (this.directoryModel_.getCurrentDirEntry());
    if (!entry) {
      return;
    }
    const sharedWithCrostini = this.crostini_.isPathShared('termina', entry);
    const sharedWithPluginVm = this.crostini_.isPathShared('PvmDefault', entry);
    if (sharedWithCrostini == this.entrySharedWithCrostini_ &&
        sharedWithPluginVm == this.entrySharedWithPluginVm_) {
      return;
    }
    this.entrySharedWithCrostini_ = sharedWithCrostini;
    this.entrySharedWithPluginVm_ = sharedWithPluginVm;

    let msg = '';
    let subpage = '';
    if (sharedWithCrostini && sharedWithPluginVm) {
      msg = 'MESSAGE_FOLDER_SHARED_WITH_CROSTINI_AND_PLUGIN_VM';
      subpage = 'app-management/pluginVm/sharedPaths';
    } else if (sharedWithCrostini) {
      msg = 'MESSAGE_FOLDER_SHARED_WITH_CROSTINI';
      subpage = 'crostini/sharedPaths';
    } else if (sharedWithPluginVm) {
      msg = 'MESSAGE_FOLDER_SHARED_WITH_PLUGIN_VM';
      subpage = 'app-management/pluginVm/sharedPaths';
    } else {
      this.filesMessage_.hidden = true;
      return;
    }

    this.filesMessage_.setContent({
      message: str(msg),
      action: str('MANAGE_TOAST_BUTTON_LABEL'),
      hidden: false,
    });
    this.filesMessage_.setSignalCallback((signal) => {
      if (signal === 'action') {
        chrome.fileManagerPrivate.openSettingsSubpage(subpage);
      }
    });
  }
}

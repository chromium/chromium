// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import type {Switch} from 'chrome://resources/cros_components/switch/switch.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {isCrosComponentsEnabled} from '../../common/js/flags.js';
import {str, strf} from '../../common/js/translations.js';
import {canBulkPinningCloudPanelShow} from '../../common/js/util.js';
import {RootType} from '../../common/js/volume_manager_types.js';
import {type State} from '../../state/state.js';
import {getStore, type Store} from '../../state/store.js';
import {XfCloudPanel} from '../../widgets/xf_cloud_panel.js';

import {ICON_TYPES} from './constants.js';
import type {DirectoryChangeEvent, DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import {EventType} from './file_selection.js';
import type {A11yAnnounce} from './ui/a11y_announce.js';
import {Command} from './ui/command.js';
import type {FileListSelectionModel} from './ui/file_list_selection_model.js';
import type {ListContainer} from './ui/list_container.js';

// Helper function that extract common pattern for getting commands associated
// with the toolbar and also deals with lack of return type information in
// assertInstaneof function.
function getCommand(selector: string, body: HTMLBodyElement): Command {
  const element = queryRequiredElement(selector, body);
  assertInstanceof(element, Command);
  return element as Command;
}

/**
 * This class controls wires toolbar UI and selection model. When selection
 * status is changed, this class changes the view of toolbar. If cancel
 * selection button is pressed, this class clears the selection.
 */
export class ToolbarController {
  // HTML Elements
  private readonly cancelSelectionButton_: HTMLElement;
  private readonly filesSelectedLabel_: HTMLElement;
  private readonly deleteButton_: HTMLElement;
  private readonly moveToTrashButton_: HTMLElement;
  private readonly restoreFromTrashButton_: HTMLElement;
  private readonly sharesheetButton_: HTMLElement;
  private readonly readOnlyIndicator_: HTMLElement;
  private readonly pinnedToggleWrapper_: HTMLElement;
  private readonly pinnedToggle_: CrToggleElement|null;
  private readonly pinnedToggleJelly_: Switch|null;
  private readonly cloudButton_: HTMLElement;
  private readonly cloudStatusIcon_: HTMLElement;
  private readonly cloudButtonIcon_: HTMLElement;
  // Commands
  private readonly deleteCommand_: Command;
  readonly moveToTrashCommand: Command;
  private readonly restoreFromTrashCommand_: Command;
  private readonly refreshCommand_: Command;
  private readonly newFolderCommand_: Command;
  private readonly invokeSharesheetCommand_: Command;
  private readonly togglePinnedCommand_: Command;
  // Other
  private bulkPinningEnabled_: boolean;
  private store_: Store;

  constructor(
      private toolbar_: HTMLElement, _navigationList: HTMLElement,
      private listContainer_: ListContainer,
      private selectionHandler_: FileSelectionHandler,
      private directoryModel_: DirectoryModel,
      private volumeManager_: VolumeManager, private a11y_: A11yAnnounce) {
    // HTML elements.
    this.cancelSelectionButton_ =
        queryRequiredElement('#cancel-selection-button', this.toolbar_);
    this.filesSelectedLabel_ =
        queryRequiredElement('#files-selected-label', this.toolbar_);
    this.deleteButton_ = queryRequiredElement('#delete-button', this.toolbar_);
    this.moveToTrashButton_ =
        queryRequiredElement('#move-to-trash-button', this.toolbar_);
    this.restoreFromTrashButton_ =
        queryRequiredElement('#restore-from-trash-button', this.toolbar_);
    this.sharesheetButton_ =
        queryRequiredElement('#sharesheet-button', this.toolbar_);
    this.readOnlyIndicator_ =
        queryRequiredElement('#read-only-indicator', this.toolbar_);
    this.pinnedToggleWrapper_ =
        queryRequiredElement('#pinned-toggle-wrapper', this.toolbar_);
    if (isCrosComponentsEnabled()) {
      this.pinnedToggle_ = null;
      this.pinnedToggleJelly_ =
          queryRequiredElement('#pinned-toggle-jelly', this.toolbar_) as Switch;
    } else {
      this.pinnedToggle_ =
          queryRequiredElement('#pinned-toggle', this.toolbar_) as
          CrToggleElement;
      this.pinnedToggleJelly_ = null;
    }
    this.cloudButton_ = queryRequiredElement('#cloud-button', this.toolbar_);
    this.cloudStatusIcon_ = queryRequiredElement(
        '#cloud-button > xf-icon[slot="suffix-icon"]', this.toolbar_);
    this.cloudButtonIcon_ = queryRequiredElement(
        '#cloud-button > xf-icon[slot="prefix-icon"]', this.toolbar_);

    // Commands.
    const body = this.toolbar_.ownerDocument.body as HTMLBodyElement;
    this.deleteCommand_ = getCommand('command#delete', body);
    this.moveToTrashCommand = getCommand('#move-to-trash', body);
    this.restoreFromTrashCommand_ = getCommand('#restore-from-trash', body);
    this.refreshCommand_ = getCommand('#refresh', body);
    this.newFolderCommand_ = getCommand('#new-folder', body);
    this.invokeSharesheetCommand_ = getCommand('#invoke-sharesheet', body);
    this.togglePinnedCommand_ = getCommand('#toggle-pinned', body);

    this.bulkPinningEnabled_ = false;

    this.store_ = getStore();
    this.store_.subscribe(this);

    this.selectionHandler_.addEventListener(
        EventType.CHANGE, this.onSelectionChanged_.bind(this));

    // Using CHANGE_THROTTLED because updateSharesheetCommand_() uses async
    // API and can update the state out-of-order specially when updating to
    // an empty selection.
    this.selectionHandler_.addEventListener(
        EventType.CHANGE_THROTTLED, this.updateSharesheetCommand_.bind(this));

    chrome.fileManagerPrivate.onAppsUpdated.addListener(
        this.updateSharesheetCommand_.bind(this));

    this.cancelSelectionButton_.addEventListener(
        'click', this.onCancelSelectionButtonClicked_.bind(this));

    this.deleteButton_.addEventListener(
        'click', this.onDeleteButtonClicked_.bind(this));

    this.moveToTrashButton_.addEventListener(
        'click', this.onMoveToTrashButtonClicked_.bind(this));

    this.restoreFromTrashButton_.addEventListener(
        'click', this.onRestoreFromTrashButtonClicked_.bind(this));

    this.sharesheetButton_.addEventListener(
        'click', this.onSharesheetButtonClicked_.bind(this));

    const cloudPanel = queryRequiredElement('xf-cloud-panel') as XfCloudPanel;
    this.cloudButton_.addEventListener('click', () => {
      this.cloudButton_.toggleAttribute('menu-shown', true);
      this.cloudButton_.toggleAttribute('aria-expanded', true);
      cloudPanel.showAt(this.cloudButton_);
    });
    cloudPanel.addEventListener(XfCloudPanel.events.PANEL_CLOSED, () => {
      this.cloudButton_.toggleAttribute('menu-shown', false);
      this.cloudButton_.toggleAttribute('aria-expanded', false);
    });

    this.togglePinnedCommand_.addEventListener(
        'checkedChange', this.updatePinnedToggle_.bind(this));

    this.moveToTrashCommand.addEventListener(
        'hiddenChange', this.updateMoveToTrashCommand_.bind(this));

    this.moveToTrashCommand.addEventListener(
        'disabledChange', this.updateMoveToTrashCommand_.bind(this));

    this.togglePinnedCommand_.addEventListener(
        'disabledChange', this.updatePinnedToggle_.bind(this));

    this.togglePinnedCommand_.addEventListener(
        'hiddenChange', this.updatePinnedToggle_.bind(this));

    (this.pinnedToggleJelly_ || this.pinnedToggle_)!.addEventListener(
        'change', this.onPinnedToggleChanged_.bind(this));

    this.directoryModel_.addEventListener(
        'directory-changed',
        this.updateCurrentDirectoryButtons_.bind(this) as
            EventListenerOrEventListenerObject);
  }

  /**
   * Updates toolbar's UI elements which are related to current directory.
   */
  private updateCurrentDirectoryButtons_(event: DirectoryChangeEvent) {
    this.updateRefreshCommand_();

    this.newFolderCommand_.canExecuteChange(this.listContainer_.currentList);

    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    const locationInfo = currentDirectory &&
        this.volumeManager_.getLocationInfo(currentDirectory);
    // Normally, isReadOnly can be used to show the label. This property
    // is always true for fake volumes (eg. Google Drive root). However, "Linux
    // files" and GuestOS volumes are fake volume on first access until the VM
    // is loaded and the mount point is initialised. The volume is technically
    // read-only since the temporary fake volume can (and should) not be
    // written to. However, showing the read only label is not appropriate since
    // the volume will become read-write once all loading has completed.
    this.readOnlyIndicator_.hidden =
        !(locationInfo && locationInfo.isReadOnly &&
          locationInfo.rootType !== RootType.CROSTINI &&
          locationInfo.rootType !== RootType.GUEST_OS);

    const newDirectory = event.detail.newDirEntry;
    if (newDirectory) {
      const locationInfo = this.volumeManager_.getLocationInfo(newDirectory);
      const bodyClassList =
          this.filesSelectedLabel_.ownerDocument.body.classList;
      if (locationInfo && locationInfo.rootType === RootType.TRASH) {
        bodyClassList.add('check-select-v1');
      } else {
        bodyClassList.remove('check-select-v1');
      }
    }
  }

  private updateRefreshCommand_() {
    this.refreshCommand_.canExecuteChange(this.listContainer_.currentList);
  }

  /**
   * Handles selection's change event to update the UI.
   */
  private onSelectionChanged_() {
    const selection = this.selectionHandler_.selection;
    this.updateRefreshCommand_();

    // Update the label "x files selected." on the header.
    let text = '';
    if (selection.totalCount === 0) {
      text = '';
    } else if (selection.totalCount === 1) {
      if (selection.directoryCount === 0) {
        text = str('ONE_FILE_SELECTED');
      } else if (selection.fileCount === 0) {
        text = str('ONE_DIRECTORY_SELECTED');
      }
    } else {
      if (selection.directoryCount === 0) {
        text = strf('MANY_FILES_SELECTED', selection.fileCount);
      } else if (selection.fileCount === 0) {
        text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
      } else {
        text = strf('MANY_ENTRIES_SELECTED', selection.totalCount);
      }
    }
    this.filesSelectedLabel_.textContent = text;

    // Update visibility of the delete and move to trash buttons.
    this.deleteButton_.hidden =
        (selection.totalCount === 0 || selection.hasReadOnlyEntry());
    // Show 'Move to Trash' rather than 'Delete' if possible. The
    // `moveToTrashCommand` needs to be set to hidden to ensure the
    // `canExecuteChange` invokes the `hiddenChange` event in the case where
    // Trash should be shown.
    this.moveToTrashButton_.hidden = true;
    this.moveToTrashCommand.disabled = true;
    if (!this.deleteButton_.hidden) {
      this.moveToTrashCommand.canExecuteChange(this.listContainer_.currentList);
    }

    // Update visibility of the restore-from-trash button.
    this.restoreFromTrashButton_.hidden = (selection.totalCount === 0) ||
        this.directoryModel_.getCurrentRootType() !== RootType.TRASH;

    this.togglePinnedCommand_.canExecuteChange(this.listContainer_.currentList);

    // Set .selecting class to containing element to change the view
    // accordingly.
    // TODO(fukino): This code changes the state of body, not the toolbar, to
    // update the checkmark visibility on grid view. This should be moved to a
    // controller which controls whole app window. Or, both toolbar and FileGrid
    // should listen to the FileSelectionHandler.
    if (this.directoryModel_.getFileListSelection().multiple) {
      const bodyClassList =
          this.filesSelectedLabel_.ownerDocument.body.classList;
      bodyClassList.toggle('selecting', selection.totalCount > 0);
      if (bodyClassList.contains('check-select') !==
          (this.directoryModel_.getFileListSelection() as
           FileListSelectionModel)
              .getCheckSelectMode()) {
        bodyClassList.toggle('check-select');
        bodyClassList.toggle('check-select-v1');
      }
    }
  }

  /**
   * Handles click event for cancel button to change the selection state.
   */
  private onCancelSelectionButtonClicked_() {
    this.directoryModel_.selectEntries([]);
    this.a11y_.speakA11yMessage(str('SELECTION_CANCELLATION'));
  }

  /**
   * Handles click event for delete button to execute the delete command.
   */
  private onDeleteButtonClicked_() {
    this.deleteCommand_.canExecuteChange(this.listContainer_.currentList);
    this.deleteCommand_.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for move to trash button to execute the move to trash
   * command.
   */
  private onMoveToTrashButtonClicked_() {
    this.moveToTrashCommand.canExecuteChange(this.listContainer_.currentList);
    this.moveToTrashCommand.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for restore from trash button to execute the restore
   * command.
   */
  private onRestoreFromTrashButtonClicked_() {
    this.restoreFromTrashCommand_.canExecuteChange(
        this.listContainer_.currentList);
    this.restoreFromTrashCommand_.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for sharesheet button to set button background color.
   */
  private onSharesheetButtonClicked_() {
    this.sharesheetButton_.setAttribute('menu-shown', '');
    this.toolbar_.ownerDocument.body.addEventListener('focusin', () => {
      this.sharesheetButton_.removeAttribute('menu-shown');
    }, {once: true});
  }

  private updateSharesheetCommand_() {
    this.invokeSharesheetCommand_.canExecuteChange(
        this.listContainer_.currentList);
  }

  private updatePinnedToggle_() {
    this.pinnedToggleWrapper_.hidden = this.togglePinnedCommand_.hidden;
    if (isCrosComponentsEnabled()) {
      this.pinnedToggleJelly_!.selected = this.togglePinnedCommand_.checked;
      this.pinnedToggleJelly_!.disabled = this.togglePinnedCommand_.disabled;
    } else {
      this.pinnedToggle_!.checked = this.togglePinnedCommand_.checked;
      this.pinnedToggle_!.disabled = this.togglePinnedCommand_.disabled;
    }
  }

  private onPinnedToggleChanged_() {
    this.togglePinnedCommand_.execute(this.listContainer_.currentList);

    // Optimistally update the command's properties so we get notified if they
    // change back.
    this.togglePinnedCommand_.checked = isCrosComponentsEnabled() ?
        this.pinnedToggleJelly_!.selected :
        this.pinnedToggle_!.checked;
  }

  private updateMoveToTrashCommand_() {
    if (!this.deleteButton_.hidden) {
      this.deleteButton_.hidden = !this.moveToTrashCommand.disabled;
      this.moveToTrashButton_.hidden = this.moveToTrashCommand.disabled;
    }
  }

  /**
   * Checks if the cloud icon should be showing or not based on the enablement
   * of the user preferences, the feature flag and the existing stage.
   */
  onStateChanged(state: State) {
    this.updateBulkPinning_(state);
  }

  /**
   * Updates the visibility of the cloud button and the "Available offline"
   * toggle based on whether the bulk pinning is enabled or not.
   */
  private updateBulkPinning_(state: State) {
    const enabled = !!state.preferences?.driveFsBulkPinningEnabled;
    const bulkPinning = state.bulkPinning;
    const isNetworkMetered = state.drive?.connectionType ===
        chrome.fileManagerPrivate.DriveConnectionStateType.METERED;
    // If bulk-pinning is enabled, the user should not be able to toggle items
    // offline.
    if (this.bulkPinningEnabled_ !== enabled) {
      this.bulkPinningEnabled_ = enabled;
      this.togglePinnedCommand_.canExecuteChange(
          this.listContainer_.currentList);
    }

    if (!canBulkPinningCloudPanelShow(bulkPinning?.stage, enabled)) {
      this.cloudButton_.hidden = true;
      return;
    }

    this.updateBulkPinningIcon_(bulkPinning, isNetworkMetered);
    this.cloudButton_.hidden = false;
  }

  /**
   * Encapsulates the logic to update the bulk pinning cloud icon and the sub
   * icons that indicate the current stage it is in.
   */
  private updateBulkPinningIcon_(
      progress: chrome.fileManagerPrivate.BulkPinProgress|undefined,
      isNetworkMetered: boolean) {
    if (isNetworkMetered) {
      this.cloudButton_.ariaLabel = str('BULK_PINNING_BUTTON_LABEL_PAUSED');
      this.cloudButtonIcon_.setAttribute('type', ICON_TYPES.CLOUD);
      this.cloudStatusIcon_.setAttribute('type', ICON_TYPES.CLOUD_PAUSED);
      this.cloudStatusIcon_.removeAttribute('size');
      return;
    }

    switch (progress?.stage) {
      case chrome.fileManagerPrivate.BulkPinStage.SYNCING:
        this.cloudButtonIcon_.setAttribute('type', ICON_TYPES.CLOUD);
        if (progress.bytesToPin === 0 ||
            progress.pinnedBytes / progress.bytesToPin === 1) {
          this.cloudButton_.ariaLabel = str('BULK_PINNING_FILE_SYNC_ON');
          this.cloudStatusIcon_.setAttribute('type', ICON_TYPES.BLANK);
        } else {
          this.cloudButton_.ariaLabel =
              str('BULK_PINNING_BUTTON_LABEL_SYNCING');
          this.cloudStatusIcon_.setAttribute('type', ICON_TYPES.CLOUD_SYNC);
        }
        break;
      case chrome.fileManagerPrivate.BulkPinStage.NOT_ENOUGH_SPACE:
        this.cloudButton_.ariaLabel = str('BULK_PINNING_BUTTON_LABEL_ISSUE');
        this.cloudButtonIcon_.setAttribute('type', ICON_TYPES.CLOUD);
        this.cloudStatusIcon_.setAttribute('type', ICON_TYPES.CLOUD_ERROR);
        break;
      case chrome.fileManagerPrivate.BulkPinStage.PAUSED_OFFLINE:
        this.cloudButton_.ariaLabel = str('BULK_PINNING_BUTTON_LABEL_OFFLINE');
        this.cloudButtonIcon_.setAttribute(
            'type', ICON_TYPES.BULK_PINNING_OFFLINE);
        this.cloudStatusIcon_.removeAttribute('type');
        this.cloudStatusIcon_.removeAttribute('size');
        break;
      case chrome.fileManagerPrivate.BulkPinStage.PAUSED_BATTERY_SAVER:
        this.cloudButton_.ariaLabel = str('BULK_PINNING_BUTTON_LABEL_PAUSED');
        this.cloudButtonIcon_.setAttribute(
            'type', ICON_TYPES.BULK_PINNING_BATTERY_SAVER);
        this.cloudStatusIcon_.removeAttribute('type');
        this.cloudStatusIcon_.removeAttribute('size');
        break;
      default:
        this.cloudButton_.ariaLabel = str('BULK_PINNING_FILE_SYNC_ON');
        this.cloudButtonIcon_.setAttribute('type', ICON_TYPES.CLOUD);
        this.cloudStatusIcon_.setAttribute('type', ICON_TYPES.BLANK);
        this.cloudStatusIcon_.removeAttribute('size');
        break;
    }
  }
}

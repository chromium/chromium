// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {updateIsInteractiveVolume} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {ProvidersModel} from './providers_model.js';

/**
 * The empty state image for the Recents folder.
 * @type {string}
 * @const
 */
const RECENTS_EMPTY_FOLDER =
    'foreground/images/files/ui/empty_folder.svg#empty_folder';

/**
 * The image shown when search returned no results.
 * @type {string}
 * @const
 */
const SEARCH_EMPTY_RESULTS =
    'foreground/images/files/ui/empty_search_results.svg#empty_search_results';

/**
 * The empty state image for the Trash folder.
 * @type {string}
 * @const
 */
const TRASH_EMPTY_FOLDER =
    'foreground/images/files/ui/empty_trash_folder.svg#empty_trash_folder';

/**
 * The reauthentication required image for ODFS. There are no files when
 * reauthentication is required (scan fails).
 * @type {string}
 * @const
 */
const ODFS_REAUTHENTICATION_REQUIRED = 'foreground/images/files/ui/' +
    'odfs_reauthentication_required.svg#odfs_reauthentication_required';

/**
 * Empty folder controller which controls the empty folder element inside
 * the file list container.
 */
export class EmptyFolderController {
  /**
   * @param {!HTMLElement} emptyFolder Empty folder element.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!ProvidersModel} providersModel Providers model.
   * @param {!FakeEntry} recentEntry Entry represents Recent view.
   */
  constructor(emptyFolder, directoryModel, providersModel, recentEntry) {
    /**
     * @private {!HTMLElement}
     */
    this.emptyFolder_ = emptyFolder;

    /**
     * @private {!DirectoryModel}
     */
    this.directoryModel_ = directoryModel;

    /**
     * Model for providers (providing extensions).
     * @private {!ProvidersModel}
     */
    this.providersModel_ = providersModel;

    /**
     * @private {!FakeEntry}
     * @const
     */
    this.recentEntry_ = recentEntry;

    /**
     * @private {!HTMLElement}
     */
    this.label_ = queryRequiredElement('.label', emptyFolder);

    /**
     * @private {!HTMLElement}
     */
    this.image_ = queryRequiredElement('.image', emptyFolder);

    /**
     * @private {boolean}
     */
    this.isScanning_ = false;

    this.directoryModel_.addEventListener(
        'scan-started', this.onScanStarted_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-failed', this.onScanFailed_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-cancelled', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-completed', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'rescan-completed', this.onScanFinished_.bind(this));
  }

  /**
   * Handles scan start.
   * @private
   */
  onScanStarted_() {
    this.isScanning_ = true;
    this.updateUI_();
  }

  /**
   * Return true if reauthentication to OneDrive is required. Request the ODFS
   * volume metadata through the special root actions request to determine if re
   * authentication is required.
   * @private
   * @param {VolumeInfo} odfsVolumeInfo
   * @return {Promise<boolean>}
   */
  async checkIfReauthenticationRequired_(odfsVolumeInfo) {
    // Request ODFS root actions to get ODFS metadata.
    return new Promise((fulfill) => {
      chrome.fileManagerPrivate.getCustomActions(
          [util.getODFSMetadataQueryEntry(odfsVolumeInfo)], customActions => {
            if (chrome.runtime.lastError) {
              console.error(
                  'Unexpectedly failed to fetch custom actions for ODFS ' +
                  'root because of: ' + chrome.runtime.lastError.message);
              fulfill(false);
            }
            // Find the reauthentication required action.
            customActions.forEach(action => {
              if (action.id ===
                      constants
                          .FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED &&
                  action.title === 'true') {
                fulfill(true);
              }
            });
            fulfill(false);
          });
    });
  }

  /**
   * Handles scan fail. If the scan failed for the ODFS volume due to
   * reauthenticaton being required, set the state of the volume as not
   * interactive.
   * @private
   */
  onScanFailed_(event) {
    this.isScanning_ = false;
    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      this.updateUI_();
      return;
    }
    // If scan did not fail for ODFS, return.
    if (!util.isOneDrive(currentVolumeInfo)) {
      this.updateUI_();
      return;
    }
    // If the error is not NO_MODIFICATION_ALLOWED_ERR, return. This is
    // equivalent to the ACCESS_DENIED error thrown by ODFS.
    if (event.error.name != util.FileError.NO_MODIFICATION_ALLOWED_ERR) {
      this.updateUI_();
      return;
    }
    // If ODFS is already non-interactive, return.
    if (!util.isInteractiveVolume(currentVolumeInfo)) {
      this.updateUI_();
      return;
    }
    // Only set ODFS to non-interactive if the ACCESS_DENIED was due to
    // reauthentication being required rather than some other access error.
    this.checkIfReauthenticationRequired_(currentVolumeInfo).then(required => {
      if (required) {
        // Set |isInteractive| to false for ODFS when reauthentication is
        // required.
        getStore().dispatch(updateIsInteractiveVolume({
          volumeId: currentVolumeInfo.volumeId,
          isInteractive: false,
        }));
      }
      this.updateUI_();
    });
  }

  /**
   * Handles scan finish.
   * @private
   */
  onScanFinished_() {
    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (util.isOneDrive(currentVolumeInfo)) {
      if (!util.isInteractiveVolume(currentVolumeInfo)) {
        // Set |isInteractive| to true for ODFS when in an authenticated state.
        getStore().dispatch(updateIsInteractiveVolume({
          volumeId: currentVolumeInfo.volumeId,
          isInteractive: true,
        }));
      }
    }
    this.isScanning_ = false;
    this.updateUI_();
  }

  /**
   * Shows the given message. It may consist of just the `title`, or
   * `title` and `description`.
   * @param {string} title
   * @param {string=} description
   * @private
   */
  showMessage_(title, description = '') {
    if (!description) {
      this.label_.appendChild(document.createTextNode(title));
      return;
    }

    const titleSpan = document.createElement('span');
    titleSpan.id = 'empty-folder-title';
    titleSpan.innerText = title;
    const descSpan = document.createElement('span');
    descSpan.innerText = description;
    this.label_.appendChild(titleSpan);
    this.label_.appendChild(document.createElement('br'));
    this.label_.appendChild(descSpan);
  }

  /**
   * Shows the ODFS reauthentication required message. Include the "Sign in"
   * and "Settings" links and set the handlers.
   * @private
   */
  showODFSReauthenticationMessage_() {
    const titleSpan = document.createElement('span');
    titleSpan.id = 'empty-folder-title';
    titleSpan.innerText = str('ONEDRIVE_LOGGED_OUT_TITLE');

    const text = document.createElement('span');
    text.innerText = str('ONEDRIVE_SIGN_IN_SUBTITLE');

    const signInLink = document.createElement('a');
    signInLink.setAttribute('class', 'sign-in');
    signInLink.innerText = str('ONEDRIVE_SIGN_IN_LINK');
    signInLink.addEventListener('click', this.onODFSSignIn_.bind(this));

    const descSpan = document.createElement('span');
    descSpan.id = 'empty-folder-desc';
    descSpan.appendChild(text);
    descSpan.appendChild(document.createElement('br'));
    descSpan.appendChild(signInLink);

    this.label_.appendChild(titleSpan);
    this.label_.appendChild(document.createElement('br'));
    this.label_.appendChild(descSpan);
  }

  /**
   * Called when "Sign in" link for ODFS reauthentication is clicked. Request
   * a new ODFS mount. ODFS will unmount the old mount if the authentication is
   * successful in the new mount.
   * @private
   */
  onODFSSignIn_() {
    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (util.isOneDrive(currentVolumeInfo) &&
        currentVolumeInfo.providerId !== undefined) {
      this.providersModel_.requestMount(currentVolumeInfo.providerId);
    }
  }

  /**
   * Updates visibility of empty folder UI.
   * @private
   */
  updateUI_() {
    const currentRootType = this.directoryModel_.getCurrentRootType();
    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();

    let svgRef = null;
    if (util.isRecentRootType(currentRootType)) {
      svgRef = RECENTS_EMPTY_FOLDER;
    } else if (currentRootType === VolumeManagerCommon.RootType.TRASH) {
      svgRef = TRASH_EMPTY_FOLDER;
    } else if (
        util.isOneDrive(currentVolumeInfo) &&
        !util.isInteractiveVolume(currentVolumeInfo)) {
      // Show ODFS reauthentication required empty state if is it
      // non-interactive.
      svgRef = ODFS_REAUTHENTICATION_REQUIRED;
    } else {
      if (util.isSearchV2Enabled()) {
        const {search} = getStore().getState();
        if (search && search.query) {
          svgRef = SEARCH_EMPTY_RESULTS;
        }
      }
    }

    const fileListModel = assert(this.directoryModel_.getFileList());

    this.label_.innerText = '';
    if (svgRef === null || this.isScanning_ || fileListModel.length > 0) {
      this.emptyFolder_.hidden = true;
      return;
    }

    const svgUseElement = this.image_.querySelector('.image > svg > use');
    svgUseElement.setAttributeNS(
        'http://www.w3.org/1999/xlink', 'xlink:href', svgRef);
    this.emptyFolder_.hidden = false;

    if (svgRef === TRASH_EMPTY_FOLDER) {
      this.showMessage_(
          str('EMPTY_TRASH_FOLDER_TITLE'), str('EMPTY_TRASH_FOLDER_DESC'));
      return;
    }

    if (svgRef == ODFS_REAUTHENTICATION_REQUIRED) {
      this.showODFSReauthenticationMessage_();
      return;
    }

    if (svgRef === SEARCH_EMPTY_RESULTS) {
      this.showMessage_(
          str('SEARCH_NO_MATCHING_RESULTS_TITLE'),
          str('SEARCH_NO_MATCHING_RESULTS_DESC'));
      return;
    }

    switch (this.recentEntry_.fileCategory) {
      case chrome.fileManagerPrivate.FileCategory.AUDIO:
        this.showMessage_(str('RECENT_EMPTY_AUDIO_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.DOCUMENT:
        this.showMessage_(str('RECENT_EMPTY_DOCUMENTS_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.IMAGE:
        this.showMessage_(str('RECENT_EMPTY_IMAGES_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.VIDEO:
        this.showMessage_(str('RECENT_EMPTY_VIDEOS_FOLDER'));
        break;
      default:
        this.showMessage_(str('RECENT_EMPTY_FOLDER'));
    }
  }
}

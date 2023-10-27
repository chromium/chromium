// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {DialogType} from '../../../common/js/dialog_type.js';
import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled, isJellyEnabled, isNewDirectoryTreeEnabled} from '../../../common/js/flags.js';
import {str, strf} from '../../../common/js/translations.js';
import {decorate, define as crUiDefine} from '../../../common/js/ui.js';
import {AllowedPaths} from '../../../common/js/volume_manager_types.js';
import {BreadcrumbContainer} from '../../../containers/breadcrumb_container.js';
import {CloudPanelContainer} from '../../../containers/cloud_panel_container.js';
import {DirectoryTreeContainer} from '../../../containers/directory_tree_container.js';
import {NudgeContainer} from '../../../containers/nudge_container.js';
import {SearchContainer} from '../../../containers/search_container.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {XfConflictDialog} from '../../../widgets/xf_conflict_dialog.js';
import {XfDlpRestrictionDetailsDialog} from '../../../widgets/xf_dlp_restriction_details_dialog.js';
import {XfPasswordDialog} from '../../../widgets/xf_password_dialog.js';
import {XfSplitter} from '../../../widgets/xf_splitter.js';
import {XfTree} from '../../../widgets/xf_tree.js';
import {BannerController} from '../banner_controller.js';
import {LaunchParam} from '../launch_param.js';
import {ProvidersModel} from '../providers_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {ActionModelUI} from './action_model_ui.js';
import {ActionsSubmenu} from './actions_submenu.js';
import {ComboButton} from './combobutton.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {DefaultTaskDialog} from './default_task_dialog.js';
import {DialogFooter} from './dialog_footer.js';
import {BaseDialog} from './dialogs.js';
import {DirectoryTree} from './directory_tree.js';
import {FileGrid} from './file_grid.js';
import {FileTable} from './file_table.js';
import {FilesAlertDialog} from './files_alert_dialog.js';
import {FilesConfirmDialog} from './files_confirm_dialog.js';
import {FilesMenuItem} from './files_menu.js';
import {GearMenu} from './gear_menu.js';
import {ImportCrostiniImageDialog} from './import_crostini_image_dialog.js';
import {InstallLinuxPackageDialog} from './install_linux_package_dialog.js';
import {ListContainer} from './list_container.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {MultiMenu} from './multi_menu.js';
import {MultiMenuButton} from './multi_menu_button.js';
import {ProgressCenterPanel} from './progress_center_panel.js';
import {ProvidersMenu} from './providers_menu.js';
import {Splitter} from './splitter.js';


/**
 * The root of the file manager's view managing the DOM of the Files app.
 * @implements {ActionModelUI}
 * @implements {A11yAnnounce}
 */
export class FileManagerUI {
  /**
   * @param {!ProvidersModel} providersModel Model for providers.
   * @param {!HTMLElement} element Top level element of the Files app.
   * @param {!LaunchParam} launchParam Launch param.
   */
  constructor(providersModel, element, launchParam) {
    // Initialize the dialog label. This should be done before constructing
    // dialog instances.
    BaseDialog.OK_LABEL = str('OK_LABEL');
    BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');

    /**
     * Top level element of the Files app.
     * @type {!HTMLElement}
     */
    this.element = element;

    /**
     * Dialog type.
     * @type {DialogType}
     * @private
     */
    this.dialogType_ = launchParam.type;

    /**
     * Alert dialog.
     * @type {!FilesAlertDialog}
     * @const
     */
    this.alertDialog = new FilesAlertDialog(this.element);

    /**
     * Confirm dialog.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.confirmDialog = new FilesConfirmDialog(this.element);

    /**
     * Confirm dialog for delete.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.deleteConfirmDialog = new FilesConfirmDialog(this.element);
    this.deleteConfirmDialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    this.deleteConfirmDialog.focusCancelButton = true;

    /**
     * Confirm dialog for emptying the trash.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.emptyTrashConfirmDialog = new FilesConfirmDialog(this.element);
    this.emptyTrashConfirmDialog.setOkLabel(str('EMPTY_TRASH_DELETE_FOREVER'));

    /**
     * Restore dialog when trying to open files that are in the trash
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.restoreConfirmDialog = new FilesConfirmDialog(this.element);
    this.restoreConfirmDialog.setOkLabel(str('RESTORE_ACTION_LABEL'));

    /**
     * Confirm dialog for file move operation.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.moveConfirmDialog = new FilesConfirmDialog(this.element);
    this.moveConfirmDialog.setOkLabel(str('CONFIRM_MOVE_BUTTON_LABEL'));

    /**
     * Confirm dialog for file copy operation.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.copyConfirmDialog = new FilesConfirmDialog(this.element);
    this.copyConfirmDialog.setOkLabel(str('CONFIRM_COPY_BUTTON_LABEL'));

    /**
     * Default task picker.
     * @type {!DefaultTaskDialog}
     * @const
     */
    this.defaultTaskPicker = new DefaultTaskDialog(this.element);

    /**
     * Dialog for installing .deb files
     * @type {!InstallLinuxPackageDialog}
     * @const
     */
    this.installLinuxPackageDialog =
        new InstallLinuxPackageDialog(this.element);

    /**
     * Dialog for import Crostini Image Files (.tini)
     * @type {!ImportCrostiniImageDialog}
     * @const
     */
    this.importCrostiniImageDialog =
        new ImportCrostiniImageDialog(this.element);

    /**
     * Dialog for formatting
     * @const @type {!HTMLElement}
     */
    this.formatDialog = queryRequiredElement('#format-dialog');

    /**
     * Dialog for password prompt
     * @type {?XfPasswordDialog}
     */
    this.passwordDialog_ = null;

    /**
     * Dialog for resolving file conflicts.
     * @type {?XfConflictDialog}
     */
    this.xfConflictDialog_ = null;

    /**
     * Dialog for DLP (Data Leak Prevention) restriction details.
     * @type {?XfDlpRestrictionDetailsDialog}
     */
    this.dlpRestrictionDetailsDialog_ = null;

    /**
     * The container element of the dialog.
     * @type {!Element}
     */
    this.dialogContainer =
        queryRequiredElement('.dialog-container', this.element);
    // @ts-ignore: error TS6133: 'event' is declared but its value is never
    // read.
    this.dialogContainer.addEventListener('relayout', (event) => {
      this.layoutChanged_();
    });

    /**
     * Context menu for texts.
     * @type {!Menu}
     * @const
     */
    // @ts-ignore: error TS2345: Argument of type '(arg0?: Object | undefined)
    // => Element' is not assignable to parameter of type 'new (...args: any) =>
    // Menu'.
    this.textContextMenu = queryDecoratedElement('#text-context-menu', Menu);

    /**
     * Breadcrumb controller.
     * @private @type {?BreadcrumbContainer}
     */
    this.breadcrumbContainer_ = null;

    /** @type {?DirectoryTreeContainer} */
    this.directoryTreeContainer = null;

    /**
     * The toolbar which contains controls.
     * @type {!Element}
     * @const
     */
    this.toolbar = queryRequiredElement('.dialog-header', this.element);

    /**
     * The tooltip element.
     * @type {!import('../../elements/files_tooltip.js').FilesTooltip}
     */
    this.filesTooltip =
        /** @type {!import('../../elements/files_tooltip.js').FilesTooltip} */ (
            document.querySelector('files-tooltip'));

    /**
     * The actionbar which contains buttons to perform actions on selected
     * file(s).
     * @type {!Element}
     * @const
     */
    this.actionbar = queryRequiredElement('#action-bar', this.toolbar);

    /**
     * The navigation list.
     * @type {!HTMLElement}
     * @const
     */
    this.dialogNavigationList =
        queryRequiredElement('.dialog-navigation-list', this.element);

    /**
     * Toggle-view button.
     * @type {!Element}
     * @const
     */
    this.toggleViewButton = queryRequiredElement('#view-button', this.element);

    /**
     * The button to sort the file list.
     * @type {!MultiMenuButton}
     * @const
     */
    this.sortButton = queryDecoratedElement('#sort-button', MultiMenuButton);

    /**
     * The button to open gear menu.
     * @type {!MultiMenuButton}
     * @const
     */
    this.gearButton = queryDecoratedElement('#gear-button', MultiMenuButton);

    /**
     * @type {!GearMenu}
     * @const
     */
    // @ts-ignore: error TS2339: Property 'menu' does not exist on type
    // 'MultiMenuButton'.
    this.gearMenu = new GearMenu(this.gearButton.menu);

    /**
     * The button to open context menu in the check-select mode.
     * @type {!MultiMenuButton}
     * @const
     */
    this.selectionMenuButton =
        queryDecoratedElement('#selection-menu-button', MultiMenuButton);

    /**
     * Directory tree.
     * @type {DirectoryTree|XfTree|null}
     */
    this.directoryTree = null;

    /**
     * Progress center panel.
     * @type {!ProgressCenterPanel}
     * @const
     */
    this.progressCenterPanel = new ProgressCenterPanel();

    /**
     * Activity feedback panel.
     * @type {!HTMLElement}
     * @const
     */
    this.activityProgressPanel =
        queryRequiredElement('#progress-panel', this.element);

    /**
     * List container.
     * @type {!ListContainer}
     */
    this.listContainer;

    /**
     * @type {!MultiMenu}
     * @const
     */
    this.fileContextMenu =
        queryDecoratedElement('#file-context-menu', MultiMenu);

    /**
     * @public @type {!FilesMenuItem}
     * @const
     */
    this.defaultTaskMenuItem =
        /** @type {!FilesMenuItem} */
        // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not
        // assignable to parameter of type 'Document | Element | HTMLElement |
        // DocumentFragment | undefined'.
        (queryRequiredElement('#default-task-menu-item', this.fileContextMenu));

    /**
     * @public @const @type {!MenuItem}
     */
    this.tasksSeparator = /** @type {!MenuItem} */
        // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not
        // assignable to parameter of type 'Document | Element | HTMLElement |
        // DocumentFragment | undefined'.
        (queryRequiredElement('#tasks-separator', this.fileContextMenu));

    /**
     * The combo button to specify the task.
     * @type {!ComboButton}
     * @const
     */
    this.taskMenuButton = queryDecoratedElement('#tasks', ComboButton);
    this.taskMenuButton.showMenu = function(shouldSetFocus) {
      // Prevent the empty menu from opening.
      // @ts-ignore: error TS2339: Property 'menu' does not exist on type
      // 'ComboButton'.
      if (!this.menu.length) {
        return;
      }
      ComboButton.prototype.showMenu.call(this, shouldSetFocus);
    };

    /**
     * Banners in the file list.
     * @type {?BannerController}
     */
    this.banners = null;

    /**
     * Dialog footer.
     * @type {!DialogFooter}
     */
    this.dialogFooter = DialogFooter.findDialogFooter(
        this.dialogType_,
        /** @type {!Document} */ (this.element.ownerDocument));

    /**
     * @public @type {!ProvidersMenu}
     * @const
     */
    this.providersMenu = new ProvidersMenu(
        // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
        // undefined) => Element' is not assignable to parameter of type 'new
        // (...args: any) => Menu'.
        providersModel, queryDecoratedElement('#providers-menu', Menu));

    /**
     * @public @type {!ActionsSubmenu}
     * @const
     */
    // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not assignable
    // to parameter of type 'Menu'.
    this.actionsSubmenu = new ActionsSubmenu(this.fileContextMenu);

    /**
     * The container that maintains the lifetime of nudges.
     * @public @type {!NudgeContainer}
     * @const
     */
    this.nudgeContainer = new NudgeContainer();

    /**
     * @type {!import('../../elements/files_toast.js').FilesToast}
     * @const
     */
    this.toast =
        /** @type {!import('../../elements/files_toast.js').FilesToast} */ (
            document.querySelector('files-toast'));

    /**
     * Container of file-type filter buttons.
     * @const @type {!HTMLElement}
     */
    this.fileTypeFilterContainer =
        queryRequiredElement('#file-type-filter-container', this.element);

    /**
     * Empty folder element inside the file list container.
     * @type {!HTMLElement}
     * @const
     */
    this.emptyFolder = queryRequiredElement('#empty-folder', this.element);

    /**
     * A hidden div that can be used to announce text to screen
     * reader/ChromeVox.
     * @private @type {!HTMLElement}
     */
    this.a11yMessage_ = queryRequiredElement('#a11y-msg', this.element);

    if (window.IN_TEST) {
      /**
       * Stores all a11y announces to be checked in tests.
       * @public @type {Array<string>}
       */
      this.a11yAnnounces = [];
    }

    // Initialize attributes.
    this.element.setAttribute('type', this.dialogType_);
    if (launchParam.allowedPaths !== AllowedPaths.ANY_PATH_OR_URL) {
      this.element.setAttribute('block-hosted-docs', '');
      this.element.setAttribute('block-encrypted', '');
    }

    // Modify UI default behavior.
    this.element.addEventListener(
        'click', this.onExternalLinkClick_.bind(this));
    this.element.addEventListener('drop', e => {
      e.preventDefault();
    });
    this.element.addEventListener('contextmenu', e => {
      e.preventDefault();
    });

    /**
     * True while FilesApp is in the process of a drag and drop. Set to true on
     * 'dragstart', set to false on 'dragend'. If CrostiniEvent
     * 'drop_failed_plugin_vm_directory_not_shared' is received during drag, we
     * show the move-to-windows-files dialog.
     *
     * @public @type {boolean}
     */
    this.dragInProcess = false;
  }

  /**
   * Gets password dialog.
   * @return {!Element}
   */
  get passwordDialog() {
    if (this.passwordDialog_) {
      return this.passwordDialog_;
    }
    this.passwordDialog_ = /** @type {!XfPasswordDialog} */ (
        document.createElement('xf-password-dialog'));
    this.element.appendChild(this.passwordDialog_);
    return this.passwordDialog_;
  }

  /**
   * Gets conflict dialog.
   * @return {!XfConflictDialog}
   */
  get conflictDialog() {
    if (this.xfConflictDialog_) {
      return this.xfConflictDialog_;
    }
    this.xfConflictDialog_ = /** @type {!XfConflictDialog} */ (
        document.createElement('xf-conflict-dialog'));
    this.element.appendChild(this.xfConflictDialog_);
    return this.xfConflictDialog_;
  }

  /**
   * Gets the DlpRestrictionDetails dialog.
   * @return {?XfDlpRestrictionDetailsDialog}
   */
  get dlpRestrictionDetailsDialog() {
    if (!isDlpEnabled()) {
      return null;
    }
    if (this.dlpRestrictionDetailsDialog_) {
      return this.dlpRestrictionDetailsDialog_;
    }
    this.dlpRestrictionDetailsDialog_ =
        /** @type {!XfDlpRestrictionDetailsDialog} */ (
            document.createElement('xf-dlp-restriction-details-dialog'));
    this.element.appendChild(this.dlpRestrictionDetailsDialog_);
    return this.dlpRestrictionDetailsDialog_;
  }

  /**
   * Initializes here elements, which are expensive or hidden in the beginning.
   *
   * @param {!FileTable} table
   * @param {!FileGrid} grid
   * @param {!VolumeManager} volumeManager
   */
  initAdditionalUI(table, grid, volumeManager) {
    // List container.
    this.listContainer = new ListContainer(
        queryRequiredElement('#list-container', this.element), table, grid,
        this.dialogType_);

    // Breadcrumb container.
    this.breadcrumbContainer_ = new BreadcrumbContainer(
        queryRequiredElement('#location-breadcrumbs', this.element));

    // Splitter.
    const splitterContainer =
        queryRequiredElement('#navigation-list-splitter', this.element);
    if (isJellyEnabled()) {
      // Remove the unused splitter <div> and wrap the tree and list with an
      // xf-splitter.
      const dialogNavList = splitterContainer.previousElementSibling;
      const dialogMain = splitterContainer.nextElementSibling;
      splitterContainer.remove();
      const splitterWidget = document.createElement('xf-splitter');
      splitterWidget.classList.add('jelly-splitter');
      splitterWidget.id = '#navigation-list-splitter';
      // @ts-ignore: error TS18047: 'dialogNavList.parentNode' is possibly
      // 'null'.
      dialogNavList.parentNode.insertBefore(splitterWidget, dialogNavList);
      // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
      // assignable to parameter of type 'Node'.
      splitterWidget.appendChild(dialogNavList);
      // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
      // assignable to parameter of type 'Node'.
      splitterWidget.appendChild(dialogMain);
      splitterWidget.addEventListener(
          XfSplitter.events.SPLITTER_DRAGMOVE, this.relayout.bind(this));
    } else {
      this.decorateSplitter_(splitterContainer);
    }

    /**
     * Search container, which controls search UI elements.
     * @type {!SearchContainer}
     * @const
     */
    this.searchContainer = new SearchContainer(
        volumeManager, queryRequiredElement('#search-wrapper', this.element),
        queryRequiredElement('#search-options-container', this.element),
        queryRequiredElement('#path-display-container', this.element),
        /*a11y=*/ this);

    if (isDriveFsBulkPinningEnabled()) {
      /**
       * @type {!CloudPanelContainer}
       * @const
       */
      this.cloudPanelContainer_ = new CloudPanelContainer(
          // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
          // assignable to parameter of type 'XfCloudPanel'.
          queryRequiredElement('xf-cloud-panel', this.element));
    }

    // Init context menus.
    // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not assignable
    // to parameter of type 'Menu'.
    contextMenuHandler.setContextMenu(grid, this.fileContextMenu);
    // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not assignable
    // to parameter of type 'Menu'.
    contextMenuHandler.setContextMenu(table.list, this.fileContextMenu);
    contextMenuHandler.setContextMenu(
        // @ts-ignore: error TS2345: Argument of type 'MultiMenu' is not
        // assignable to parameter of type 'Menu'.
        queryRequiredElement('.drive-welcome.page'), this.fileContextMenu);

    // Add window resize handler.
    document.defaultView?.addEventListener('resize', this.relayout.bind(this));

    // Add global pointer-active handler.
    const rootElement = document.documentElement;
    let pointerActive = ['pointerdown', 'pointerup', 'dragend', 'touchend'];
    if (window.IN_TEST) {
      pointerActive = pointerActive.concat(['mousedown', 'mouseup']);
    }
    pointerActive.forEach((eventType) => {
      document.addEventListener(eventType, (e) => {
        if (/down$/.test(e.type) === false) {
          rootElement.classList.toggle('pointer-active', false);
        } else if (
            /** @type {!PointerEvent}*/ (e).pointerType !==
            'touch') {  // http://crbug.com/1311472
          rootElement.classList.toggle('pointer-active', true);
        }
      }, true);
    });

    // Add global drag-drop-active handler.
    /** @type {?EventTarget} */
    let activeDropTarget = null;
    ['dragenter', 'dragleave', 'drop'].forEach((eventType) => {
      document.addEventListener(eventType, (event) => {
        const dragDropActive = 'drag-drop-active';
        if (event.type === 'dragenter') {
          rootElement.classList.add(dragDropActive);
          activeDropTarget = event.target;
        } else if (activeDropTarget === event.target) {
          rootElement.classList.remove(dragDropActive);
          activeDropTarget = null;
        }
      });
    });

    document.addEventListener('dragstart', () => {
      this.dragInProcess = true;
    });
    document.addEventListener('dragend', () => {
      this.dragInProcess = false;
    });

    // Observe the dialog header content box size: the breadcrumb and action
    // bar buttons can become wide enough to extend past the available viewport,
    // and this.layoutChanged_() is used to clamp their size to the viewport.
    const resizeObserver = new ResizeObserver(() => this.layoutChanged_());
    resizeObserver.observe(queryRequiredElement('div.dialog-header'));
  }

  /**
   * Initializes the focus.
   */
  initUIFocus() {
    // Set the initial focus. When there is no focus, the active element is the
    // <body>.
    let targetElement = null;
    if (this.dialogType_ == DialogType.SELECT_SAVEAS_FILE) {
      targetElement = this.dialogFooter.filenameInput;
    } else if (
        this.listContainer.currentListType !=
        ListContainer.ListType.UNINITIALIZED) {
      targetElement = this.listContainer.currentList;
    }

    if (targetElement) {
      targetElement.focus();
    }
  }

  /**
   * TODO(hirono): Merge the method into initAdditionalUI.
   * @param {!(DirectoryTree|DirectoryTreeContainer)} directoryTree
   *
   * @suppress {checkTypes} closure can't cast Element to XfTree.
   */
  initDirectoryTree(directoryTree) {
    if (isNewDirectoryTreeEnabled()) {
      this.directoryTreeContainer =
          /** @type {!DirectoryTreeContainer} */ (directoryTree);
      this.directoryTree =
          /** @type {!XfTree} */ (this.directoryTreeContainer.tree);

      this.directoryTreeContainer.contextMenuForRootItems =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#roots-context-menu', Menu);
      this.directoryTreeContainer.contextMenuForSubitems =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#directory-tree-context-menu', Menu);
      this.directoryTreeContainer.contextMenuForDisabledItems =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#disabled-context-menu', Menu);
    } else {
      this.directoryTree = /** @type {!DirectoryTree} */ (directoryTree);

      // Set up the context menu for the volume/shortcut items in directory
      // tree.
      this.directoryTree.contextMenuForRootItems =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#roots-context-menu', Menu);
      this.directoryTree.contextMenuForSubitems =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#directory-tree-context-menu', Menu);
      this.directoryTree.disabledContextMenu =
          // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
          // undefined) => Element' is not assignable to parameter of type 'new
          // (...args: any) => Menu | null'.
          queryDecoratedElement('#disabled-context-menu', Menu);

      // The context menu event that is created via keyboard navigation is
      // dispatched to the `directoryTree` however the tree items actually have
      // the context menu handlers. To ensure they receive the event, recompute
      // their location and re-dispatch the "contextmenu" event to the item that
      // is selected.
      this.directoryTree.addEventListener('contextmenu', e => {
        const selectedItem = this.directoryTree?.selectedItem?.rowElement;
        if (!selectedItem) {
          return;
        }
        const domRect = selectedItem.getBoundingClientRect();
        const x = domRect.x + (domRect.width / 2);
        const y = domRect.y + (domRect.height / 2);
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.directoryTree.selectedItem.dispatchEvent(
            new PointerEvent(e.type, {...e, clientX: x, clientY: y}));
      });
    }
  }

  /**
   * TODO(mtomasz): Merge the method into initAdditionalUI if possible.
   * @param {!BannerController} banners
   */
  initBanners(banners) {
    this.banners = banners;
    this.banners.addEventListener('relayout', this.relayout.bind(this));
  }

  /**
   * Attaches files tooltip.
   */
  attachFilesTooltip() {
    this.filesTooltip.addTargets(document.querySelectorAll('[has-tooltip]'));
  }

  /**
   * Initialize files menu items. This method must be called after all files
   * menu items are decorated as MenuItem.
   */
  decorateFilesMenuItems() {
    const filesMenuItems =
        document.querySelectorAll('cr-menu.files-menu > cr-menu-item');

    for (let i = 0; i < filesMenuItems.length; i++) {
      const filesMenuItem = filesMenuItems[i];
      assertInstanceof(filesMenuItem, MenuItem);
      // @ts-ignore: error TS2345: Argument of type 'Element | undefined' is not
      // assignable to parameter of type 'string | Element'.
      decorate(filesMenuItem, FilesMenuItem);
    }
  }

  /**
   * Relayouts the UI.
   */
  relayout() {
    // May not be available during initialization.
    if (this.listContainer.currentListType !==
        ListContainer.ListType.UNINITIALIZED) {
      this.listContainer.currentView.relayout();
    }
    if (!isNewDirectoryTreeEnabled() && this.directoryTree) {
      // @ts-ignore: error TS2339: Property 'relayout' does not exist on type
      // 'XfTree | DirectoryTree'.
      this.directoryTree?.relayout();
    }
  }

  /**
   * Handles the 'relayout' event to set sizing of the dialog main panel.
   *
   * @private
   */
  layoutChanged_() {
    // The Jelly splitter uses flexbox, no need for this.
    if (isJellyEnabled() || this.scrollRAFActive_ === true) {
      return;
    }

    /**
     * True if a scroll RAF is active: scroll events are frequent and serviced
     * using RAF to throttle our processing of these events.
     * @type {boolean}
     */
    this.scrollRAFActive_ = true;

    window.requestAnimationFrame(() => {
      this.scrollRAFActive_ = false;

      const mainWindow = document.querySelector('.dialog-container');
      const navigationList = document.querySelector('.dialog-navigation-list');
      const splitter = document.querySelector('.splitter');
      const dialogMain = document.querySelector('.dialog-main');

      // Check the width of the tree and splitter and set the main panel width
      // to the remainder if it's too wide.
      // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on type
      // 'Element'.
      const mainWindowWidth = mainWindow.offsetWidth;
      // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on type
      // 'Element'.
      const navListWidth = navigationList.offsetWidth;
      // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
      // assignable to parameter of type 'Element'.
      const splitStyle = window.getComputedStyle(splitter);
      const splitMargin = parseInt(splitStyle.marginRight, 10) +
          parseInt(splitStyle.marginLeft, 10);
      // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on type
      // 'Element'.
      const splitWidth = splitter.offsetWidth + splitMargin;
      // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on type
      // 'Element'.
      const dialogMainWidth = dialogMain.offsetWidth;
      // @ts-ignore: error TS2339: Property 'style' does not exist on type
      // 'Element'.
      if (!dialogMain.style.width ||
          (navListWidth + splitWidth + dialogMainWidth) > mainWindowWidth) {
        // @ts-ignore: error TS2339: Property 'style' does not exist on type
        // 'Element'.
        dialogMain.style.width =
            (mainWindowWidth - navListWidth - splitWidth) + 'px';
      }
    });
  }

  /**
   * Sets the current list type.
   * @param {ListContainer.ListType} listType New list type.
   */
  setCurrentListType(listType) {
    this.listContainer.setCurrentListType(listType);

    const isListView = (listType === ListContainer.ListType.DETAIL);
    this.toggleViewButton.classList.toggle('thumbnail', isListView);

    const label = isListView ? str('CHANGE_TO_THUMBNAILVIEW_BUTTON_LABEL') :
                               str('CHANGE_TO_LISTVIEW_BUTTON_LABEL');
    this.toggleViewButton.setAttribute('aria-label', label);
    this.relayout();
  }

  /**
   * Overrides default handling for clicks on hyperlinks.
   * In a packaged apps links with target='_blank' open in a new tab by
   * default, other links do not open at all.
   *
   * @param {!Event} event Click event.
   * @private
   */
  onExternalLinkClick_(event) {
    // @ts-ignore: error TS2339: Property 'href' does not exist on type
    // 'EventTarget'.
    if (event.target.tagName != 'A' || !event.target.href) {
      return;
    }

    if (this.dialogType_ != DialogType.FULL_PAGE) {
      this.dialogFooter.cancelButton.click();
    }
  }

  /**
   * Decorates the given splitter element.
   * @param {!HTMLElement} splitterElement
   * @param {boolean=} opt_resizeNextElement
   * @private
   */
  decorateSplitter_(splitterElement, opt_resizeNextElement) {
    const self = this;
    const FileSplitter = Splitter;
    const customSplitter = crUiDefine('div');

    customSplitter.prototype = {
      __proto__: FileSplitter.prototype,

      // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
      handleSplitterDragStart: function(e) {
        FileSplitter.prototype.handleSplitterDragStart.apply(this, arguments);
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type '{ __proto__: any; handleSplitterDragStart: (e: any, ...args:
        // any[]) => void; handleSplitterDragMove: (deltaX: any, ...args: any[])
        // => void; handleSplitterDragEnd: (e: any, ...args: any[]) => void; }'.
        this.ownerDocument.documentElement.classList.add('col-resize');
      },

      // @ts-ignore: error TS7006: Parameter 'deltaX' implicitly has an 'any'
      // type.
      handleSplitterDragMove: function(deltaX) {
        FileSplitter.prototype.handleSplitterDragMove.apply(this, arguments);
        self.relayout();
      },

      // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
      handleSplitterDragEnd: function(e) {
        FileSplitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type '{ __proto__: any; handleSplitterDragStart: (e: any, ...args:
        // any[]) => void; handleSplitterDragMove: (deltaX: any, ...args: any[])
        // => void; handleSplitterDragEnd: (e: any, ...args: any[]) => void; }'.
        this.ownerDocument.documentElement.classList.remove('col-resize');
      },
    };

    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Object'.
    /** @type Object */ (customSplitter).decorate(splitterElement);
    // @ts-ignore: error TS2339: Property 'resizeNextElement' does not exist on
    // type 'HTMLElement'.
    splitterElement.resizeNextElement = !!opt_resizeNextElement;
  }

  /**
   * Mark |element| with "loaded" attribute to indicate that File Manager has
   * finished loading.
   */
  addLoadedAttribute() {
    this.element.setAttribute('loaded', '');
  }

  /**
   * Sets up and shows the alert to inform a user the task is opened in the
   * desktop of the running profile.
   *
   * @param {Array<Entry>} entries List of opened entries.
   */
  showOpenInOtherDesktopAlert(entries) {
    if (!entries.length) {
      return;
    }
    chrome.fileManagerPrivate.getProfiles(
        // @ts-ignore: error TS6133: 'displayedId' is declared but its value is
        // never read.
        (profiles, currentId, displayedId) => {
          // Find strings.
          let displayName;
          for (let i = 0; i < profiles.length; i++) {
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            if (profiles[i].profileId === currentId) {
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              displayName = profiles[i].displayName;
              break;
            }
          }
          if (!displayName) {
            console.warn('Display name is not found.');
            return;
          }

          const title = entries.length > 1 ?
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              entries[0].name + '\u2026' /* ellipsis */ :
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              entries[0].name;
          const message = strf(
              entries.length > 1 ? 'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                                   'OPEN_IN_OTHER_DESKTOP_MESSAGE',
              displayName, currentId);

          // Show the dialog.
          this.alertDialog.showWithTitle(title, message, null, null, null);
        });
  }

  /**
   * Shows confirmation dialog and handles user interaction.
   * @param {boolean} isMove true if the operation is move. false if copy.
   * @param {!Array<string>} messages The messages to show in the dialog.
   *     box.
   * @return {!Promise<boolean>}
   */
  showConfirmationDialog(isMove, messages) {
    // @ts-ignore: error TS7034: Variable 'dialog' implicitly has type 'any' in
    // some locations where its type cannot be determined.
    let dialog = null;
    if (isMove) {
      dialog = this.moveConfirmDialog;
    } else {
      dialog = this.copyConfirmDialog;
    }
    // @ts-ignore: error TS6133: 'reject' is declared but its value is never
    // read.
    return new Promise((resolve, reject) => {
      // @ts-ignore: error TS7005: Variable 'dialog' implicitly has an 'any'
      // type.
      dialog.show(
          messages.join(' '),
          () => {
            resolve(true);
          },
          () => {
            resolve(false);
          });
    });
  }

  /**
   * Send a text to screen reader/Chromevox without displaying the text in the
   * UI.
   * @param {string} text Text to be announced by screen reader, which should be
   * already translated.
   */
  speakA11yMessage(text) {
    // Screen reader only reads if the content changes, so clear the content
    // first.
    this.a11yMessage_.textContent = '';
    this.a11yMessage_.textContent = text;
    if (window.IN_TEST) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.a11yAnnounces.push(text);
    }
  }
}

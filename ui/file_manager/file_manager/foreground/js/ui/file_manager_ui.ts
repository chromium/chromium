// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../elements/icons.html.js';

import {assertInstanceof} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {queryDecoratedElement, queryRequiredElement} from '../../../common/js/dom_utils.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {isDlpEnabled} from '../../../common/js/flags.js';
import {str, strf} from '../../../common/js/translations.js';
import {AllowedPaths} from '../../../common/js/volume_manager_types.js';
import {BreadcrumbContainer} from '../../../containers/breadcrumb_container.js';
import {CloudPanelContainer} from '../../../containers/cloud_panel_container.js';
import type {DirectoryTreeContainer} from '../../../containers/directory_tree_container.js';
import {NudgeContainer} from '../../../containers/nudge_container.js';
import {SearchContainer} from '../../../containers/search_container.js';
import {DialogType} from '../../../state/state.js';
import type {XfCloudPanel} from '../../../widgets/xf_cloud_panel.js';
import type {XfConflictDialog} from '../../../widgets/xf_conflict_dialog.js';
import type {XfDlpRestrictionDetailsDialog} from '../../../widgets/xf_dlp_restriction_details_dialog.js';
import type {XfPasswordDialog} from '../../../widgets/xf_password_dialog.js';
import {XfSplitter} from '../../../widgets/xf_splitter.js';
import type {XfTree} from '../../../widgets/xf_tree.js';
import type {FilesFormatDialog} from '../../elements/files_format_dialog.js';
import type {FilesToast} from '../../elements/files_toast.js';
import type {FilesTooltip} from '../../elements/files_tooltip.js';
import type {BannerController} from '../banner_controller.js';
import type {LaunchParam} from '../launch_param.js';
import type {ProvidersModel} from '../providers_model.js';

import {ActionsSubmenu} from './actions_submenu.js';
import {ComboButton} from './combobutton.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {DefaultTaskDialog} from './default_task_dialog.js';
import {DialogFooter} from './dialog_footer.js';
import {BaseDialog} from './dialogs.js';
import type {FileGrid} from './file_grid.js';
import type {FileTable} from './file_table.js';
import {FilesAlertDialog} from './files_alert_dialog.js';
import {FilesConfirmDialog} from './files_confirm_dialog.js';
import {FilesMenuItem} from './files_menu.js';
import {GearMenu} from './gear_menu.js';
import {ImportCrostiniImageDialog} from './import_crostini_image_dialog.js';
import {InstallLinuxPackageDialog} from './install_linux_package_dialog.js';
import {ListContainer, ListType} from './list_container.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {MultiMenu} from './multi_menu.js';
import {MultiMenuButton} from './multi_menu_button.js';
import {ProgressCenterPanel} from './progress_center_panel.js';
import {ProvidersMenu} from './providers_menu.js';



/**
 * The root of the file manager's view managing the DOM of the Files app.
 */
// eslint-disable-next-line @typescript-eslint/naming-convention
export class FileManagerUI {
  /**
   * Dialog type.
   */
  private dialogType_: DialogType;

  /**
   * Alert dialog. Overrides ActionModelUI declaration.
   */
  readonly alertDialog: FilesAlertDialog;

  /**
   * List container. Overrides ActionModelUI declaration.
   */
  private listContainer_: ListContainer|null = null;

  /**
   * Confirm dialog.
   */
  readonly confirmDialog: FilesConfirmDialog;

  /**
   * Confirm dialog for delete.
   */
  readonly deleteConfirmDialog: FilesConfirmDialog;

  /**
   * Confirm dialog for emptying the trash.
   */
  readonly emptyTrashConfirmDialog: FilesConfirmDialog;

  /**
   * Restore dialog when trying to open files that are in the trash
   */
  readonly restoreConfirmDialog: FilesConfirmDialog;

  /**
   * Confirm dialog for file move operation.
   */
  readonly moveConfirmDialog: FilesConfirmDialog;

  /**
   * Confirm dialog for file copy operation.
   */
  readonly copyConfirmDialog: FilesConfirmDialog;

  /**
   * Default task picker.
   * TODO(b:289003444): Make it readonly after fixing tests.
   */
  /* readonly */ defaultTaskPicker: DefaultTaskDialog;

  /**
   * Dialog for installing .deb files
   * TODO(b:289003444): Make it readonly after fixing tests.
   */
  /* readonly */ installLinuxPackageDialog: InstallLinuxPackageDialog;

  /**
   * Dialog for import Crostini Image Files (.tini)
   * TODO(b:289003444): Make it readonly after fixing tests.
   */
  /* readonly */ importCrostiniImageDialog: ImportCrostiniImageDialog;

  /**
   * Dialog for formatting
   */
  readonly formatDialog: FilesFormatDialog;

  /**
   * Dialog for password prompt
   */
  private passwordDialog_: XfPasswordDialog|null = null;

  /**
   * Dialog for resolving file conflicts.
   */
  private xfConflictDialog_: XfConflictDialog|null = null;

  /**
   * Dialog for DLP (Data Leak Prevention) restriction details.
   */
  private dlpRestrictionDetailsDialog_: XfDlpRestrictionDetailsDialog|null =
      null;

  protected cloudPanelContainer_: CloudPanelContainer|null = null;

  /**
   * Breadcrumb controller.
   */
  protected breadcrumbContainer_: BreadcrumbContainer|null = null;

  /**
   * The container element of the dialog.
   */
  dialogContainer: HTMLDialogElement;

  /**
   * Context menu for texts.
   */
  readonly textContextMenu: Menu;

  directoryTreeContainer: DirectoryTreeContainer|null = null;

  /**
   * The toolbar which contains controls.
   */
  readonly toolbar: HTMLElement;

  /**
   * The tooltip element.
   */
  filesTooltip: FilesTooltip;

  /**
   * The actionbar which contains buttons to perform actions on selected
   * file(s).
   */
  readonly actionbar: HTMLElement;

  /**
   * The navigation list.
   */
  readonly dialogNavigationList: HTMLElement;

  /**
   * Toggle-view button.
   */
  readonly toggleViewButton: HTMLElement;

  /**
   * The button to sort the file list.
   */
  readonly sortButton: MultiMenuButton;

  /**
   * The button to open gear menu.
   */
  readonly gearButton: MultiMenuButton;

  readonly gearMenu: GearMenu;

  /**
   * The button to open context menu in the check-select mode.
   */
  readonly selectionMenuButton: MultiMenuButton;

  /**
   * Directory tree.
   */
  directoryTree: XfTree|null = null;

  /**
   * Progress center panel.
   */
  readonly progressCenterPanel: ProgressCenterPanel;

  /**
   * Activity feedback panel.
   */
  readonly activityProgressPanel: HTMLElement;

  readonly fileContextMenu: MultiMenu;

  defaultTaskMenuItem: FilesMenuItem;

  readonly tasksSeparator: MenuItem;

  /**
   * The combo button to specify the task.
   */
  readonly taskMenuButton: ComboButton;

  /**
   * Banners in the file list.
   */
  banners: BannerController|null = null;

  /**
   * Dialog footer.
   */
  dialogFooter: DialogFooter;

  readonly providersMenu: ProvidersMenu;

  readonly actionsSubmenu: ActionsSubmenu;

  /**
   * The container that maintains the lifetime of nudges.
   */
  readonly nudgeContainer: NudgeContainer;

  readonly toast: FilesToast;

  /**
   * Container of file-type filter buttons.
   */
  readonly fileTypeFilterContainer: HTMLElement;

  /**
   * Empty folder element inside the file list container.
   */
  readonly emptyFolder: HTMLElement;

  /**
   * A hidden div that can be used to announce text to screen
   * reader/ChromeVox.
   */
  private a11yMessage_: HTMLElement;

  searchContainer: SearchContainer|null = null;

  a11yAnnounces: string[]|null = null;

  /**
   * True while FilesApp is in the process of a drag and drop. Set to true on
   * 'dragstart', set to false on 'dragend'. If CrostiniEvent
   * 'drop_failed_plugin_vm_directory_not_shared' is received during drag, we
   * show the move-to-windows-files dialog.
   */
  dragInProcess: boolean = false;

  /**
   * @param providersModel Model for providers.
   * @param element Top level element of the Files app.
   * @param launchParam Launch param.
   */
  constructor(
      providersModel: ProvidersModel, public element: HTMLElement,
      launchParam: LaunchParam) {
    // Initialize the dialog label. This should be done before constructing
    // dialog instances.
    BaseDialog.okLabel = str('OK_LABEL');
    BaseDialog.cancelLabel = str('CANCEL_LABEL');

    this.dialogType_ = launchParam.type;

    this.alertDialog = new FilesAlertDialog(this.element);

    this.confirmDialog = new FilesConfirmDialog(this.element);

    this.deleteConfirmDialog = new FilesConfirmDialog(this.element);
    this.deleteConfirmDialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    this.deleteConfirmDialog.focusCancelButton = true;

    this.emptyTrashConfirmDialog = new FilesConfirmDialog(this.element);
    this.emptyTrashConfirmDialog.setOkLabel(str('EMPTY_TRASH_DELETE_FOREVER'));

    this.restoreConfirmDialog = new FilesConfirmDialog(this.element);
    this.restoreConfirmDialog.setOkLabel(str('RESTORE_ACTION_LABEL'));

    this.moveConfirmDialog = new FilesConfirmDialog(this.element);
    this.moveConfirmDialog.setOkLabel(str('CONFIRM_MOVE_BUTTON_LABEL'));

    this.copyConfirmDialog = new FilesConfirmDialog(this.element);
    this.copyConfirmDialog.setOkLabel(str('CONFIRM_COPY_BUTTON_LABEL'));

    this.defaultTaskPicker = new DefaultTaskDialog(this.element);

    this.installLinuxPackageDialog =
        new InstallLinuxPackageDialog(this.element);

    this.importCrostiniImageDialog =
        new ImportCrostiniImageDialog(this.element);

    this.formatDialog =
        queryRequiredElement('#format-dialog') as FilesFormatDialog;

    this.dialogContainer =
        queryRequiredElement('.dialog-container', this.element) as
        HTMLDialogElement;

    this.textContextMenu = queryDecoratedElement('#text-context-menu', Menu);

    this.toolbar = queryRequiredElement('.dialog-header', this.element);

    this.filesTooltip = document.querySelector<FilesTooltip>('files-tooltip')!;

    this.actionbar = queryRequiredElement('#action-bar', this.toolbar);

    this.dialogNavigationList =
        queryRequiredElement('.dialog-navigation-list', this.element);

    this.toggleViewButton = queryRequiredElement('#view-button', this.element);

    this.sortButton = queryDecoratedElement('#sort-button', MultiMenuButton);

    this.gearButton = queryDecoratedElement('#gear-button', MultiMenuButton);

    this.gearMenu = new GearMenu(this.gearButton.menu!);

    this.selectionMenuButton =
        queryDecoratedElement('#selection-menu-button', MultiMenuButton);

    this.progressCenterPanel = new ProgressCenterPanel();

    this.activityProgressPanel =
        queryRequiredElement('#progress-panel', this.element);

    this.fileContextMenu =
        queryDecoratedElement('#file-context-menu', MultiMenu);

    this.defaultTaskMenuItem =
        queryRequiredElement('#default-task-menu-item', this.fileContextMenu) as
        FilesMenuItem;

    this.tasksSeparator =
        queryRequiredElement('#tasks-separator', this.fileContextMenu) as
        MenuItem;

    this.taskMenuButton = queryDecoratedElement('#tasks', ComboButton);
    this.taskMenuButton.showMenu = function(shouldSetFocus) {
      // Prevent the empty menu from opening.
      if (!this.menu?.length) {
        return;
      }
      ComboButton.prototype.showMenu.call(this, shouldSetFocus);
    };

    this.dialogFooter = DialogFooter.findDialogFooter(
        this.dialogType_, this.element.ownerDocument);

    this.providersMenu = new ProvidersMenu(
        providersModel, queryDecoratedElement('#providers-menu', Menu));

    this.actionsSubmenu = new ActionsSubmenu(this.fileContextMenu);

    this.nudgeContainer = new NudgeContainer();

    this.toast = document.querySelector<FilesToast>('files-toast')!;

    this.fileTypeFilterContainer =
        queryRequiredElement('#file-type-filter-container', this.element);

    this.emptyFolder = queryRequiredElement('#empty-folder', this.element);

    this.a11yMessage_ = queryRequiredElement('#a11y-msg', this.element);

    if (window.IN_TEST) {
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
  }

  get listContainer(): ListContainer {
    return this.listContainer_!;
  }

  /**
   * Gets password dialog.
   */
  get passwordDialog(): HTMLElement {
    if (this.passwordDialog_) {
      return this.passwordDialog_;
    }
    this.passwordDialog_ = document.createElement('xf-password-dialog');
    this.element.appendChild(this.passwordDialog_);
    return this.passwordDialog_;
  }

  /**
   * Gets conflict dialog.
   */
  get conflictDialog(): XfConflictDialog {
    if (this.xfConflictDialog_) {
      return this.xfConflictDialog_;
    }
    this.xfConflictDialog_ = document.createElement('xf-conflict-dialog');
    this.element.appendChild(this.xfConflictDialog_);
    return this.xfConflictDialog_;
  }

  /**
   * Gets the DlpRestrictionDetails dialog.
   */
  get dlpRestrictionDetailsDialog(): null|XfDlpRestrictionDetailsDialog {
    if (!isDlpEnabled()) {
      return null;
    }
    if (this.dlpRestrictionDetailsDialog_) {
      return this.dlpRestrictionDetailsDialog_;
    }
    this.dlpRestrictionDetailsDialog_ =
        document.createElement('xf-dlp-restriction-details-dialog') as
        XfDlpRestrictionDetailsDialog;
    this.element.appendChild(this.dlpRestrictionDetailsDialog_);
    return this.dlpRestrictionDetailsDialog_;
  }

  /**
   * Initializes here elements, which are expensive or hidden in the beginning.
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  initAdditionalUI(
      table: FileTable, grid: FileGrid, volumeManager: VolumeManager) {
    // List container.
    this.listContainer_ = new ListContainer(
        queryRequiredElement('#list-container', this.element), table, grid,
        this.dialogType_);

    // Breadcrumb container.
    this.breadcrumbContainer_ = new BreadcrumbContainer(
        queryRequiredElement('#location-breadcrumbs', this.element));

    // Splitter.
    const splitterContainer =
        queryRequiredElement('#navigation-list-splitter', this.element);
    splitterContainer.addEventListener(
        XfSplitter.events.SPLITTER_DRAGMOVE, this.relayout.bind(this));


    /**
     * Search container, which controls search UI elements.
     */
    this.searchContainer = new SearchContainer(
        volumeManager, queryRequiredElement('#search-wrapper', this.element),
        queryRequiredElement('#search-options-container', this.element),
        queryRequiredElement('#path-display-container', this.element),
        /*a11y=*/ this);

    this.cloudPanelContainer_ = new CloudPanelContainer(
        queryRequiredElement('xf-cloud-panel', this.element) as XfCloudPanel);

    // Init context menus.
    contextMenuHandler.setContextMenu(grid, this.fileContextMenu);
    contextMenuHandler.setContextMenu(table.list, this.fileContextMenu);
    contextMenuHandler.setContextMenu(
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
        } else if ((e as PointerEvent).pointerType !== 'touch') {
          // http://crbug.com/1311472
          rootElement.classList.toggle('pointer-active', true);
        }
      }, true);
    });

    // Add global drag-drop-active handler.
    let activeDropTarget: EventTarget|null = null;
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
  }

  /**
   * Initializes the focus.
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  initUIFocus() {
    // Set the initial focus. When there is no focus, the active element is the
    // <body>.
    let targetElement = null;
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      targetElement = this.dialogFooter.filenameInput;
    } else if (this.listContainer.currentListType !== ListType.UNINITIALIZED) {
      targetElement = this.listContainer.currentList;
    }

    if (targetElement) {
      targetElement.focus();
    }
  }

  /**
   * TODO(hirono): Merge the method into initAdditionalUI.
   */
  initDirectoryTree(directoryTree: DirectoryTreeContainer) {
    this.directoryTreeContainer = directoryTree;
    this.directoryTree = this.directoryTreeContainer.tree;

    this.directoryTreeContainer.contextMenuForRootItems =
        queryDecoratedElement('#roots-context-menu', Menu);
    this.directoryTreeContainer.contextMenuForSubitems =
        queryDecoratedElement('#directory-tree-context-menu', Menu);
    this.directoryTreeContainer.contextMenuForDisabledItems =
        queryDecoratedElement('#disabled-context-menu', Menu);
  }

  /**
   * TODO(mtomasz): Merge the method into initAdditionalUI if possible.
   */
  initBanners(banners: BannerController) {
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

    for (const filesMenuItem of filesMenuItems) {
      assertInstanceof(filesMenuItem, MenuItem);
      crInjectTypeAndInit(filesMenuItem, FilesMenuItem);
    }
  }

  /**
   * Relayouts the UI.
   */
  relayout() {
    // May not be available during initialization.
    if (this.listContainer.currentListType !== ListType.UNINITIALIZED) {
      this.listContainer.currentView.relayout();
    }
  }

  /**
   * Sets the current list type.
   * @param listType New list type.
   */
  setCurrentListType(listType: ListType) {
    this.listContainer.setCurrentListType(listType);

    const isListView = (listType === ListType.DETAIL);
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
   * @param event Click event.
   */
  private onExternalLinkClick_(event: Event) {
    const target = event.target;
    if (!(target instanceof HTMLElement) || target.tagName !== 'A' ||
        !('href' in target)) {
      return;
    }

    if (this.dialogType_ !== DialogType.FULL_PAGE) {
      this.dialogFooter.cancelButton.click();
    }
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
   * @param entries List of opened entries.
   */
  showOpenInOtherDesktopAlert(entries: Array<Entry|FilesAppEntry>) {
    if (!entries.length) {
      return;
    }
    chrome.fileManagerPrivate.getProfiles(
        (response: chrome.fileManagerPrivate.ProfilesResponse) => {
          //  Find strings.
          let displayName;
          for (const profile of response.profiles) {
            if (profile.profileId === response.currentProfileId) {
              displayName = profile.displayName;
              break;
            }
          }
          if (!displayName) {
            console.warn('Display name is not found.');
            return;
          }

          const title = entries.length > 1 ?
              entries[0]!.name + '\u2026' /* ellipsis */ :
              entries[0]!.name;
          const message = strf(
              entries.length > 1 ? 'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                                   'OPEN_IN_OTHER_DESKTOP_MESSAGE',
              displayName, response.currentProfileId);

          // Show the dialog.
          this.alertDialog.showWithTitle(title, message);
        });
  }

  /**
   * Shows confirmation dialog and handles user interaction.
   * @param isMove true if the operation is move. false if copy.
   * @param messages The messages to show in the dialog.
   *     box.
   */
  showConfirmationDialog(isMove: boolean, messages: string[]):
      Promise<boolean> {
    const dialog = isMove ? this.moveConfirmDialog : this.copyConfirmDialog;
    return new Promise<boolean>((resolve) => {
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
   * @param text Text to be announced by screen reader, which should be
   * already translated.
   */
  speakA11yMessage(text: string) {
    // Screen reader only reads if the content changes, so clear the content
    // first.
    this.a11yMessage_.textContent = '';
    this.a11yMessage_.textContent = text;
    // this.a11yAnnounces is not null only during tests; see constructor.
    if (this.a11yAnnounces) {
      this.a11yAnnounces.push(text);
    }
  }
}

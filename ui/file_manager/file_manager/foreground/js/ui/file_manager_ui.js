// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The root of the file manager's view managing the DOM of the Files app.
 *
 * @param {!ProvidersModel} providersModel Model for providers.
 * @param {!HTMLElement} element Top level element of the Files app.
 * @param {!LaunchParam} launchParam Launch param.
 * @constructor
 * @struct
 */
function FileManagerUI(providersModel, element, launchParam) {
  // Pre-populate the static localized strings.
  i18nTemplate.process(element.ownerDocument, loadTimeData);

  // Initialize the dialog label. This should be done before constructing dialog
  // instances.
  cr.ui.dialogs.BaseDialog.OK_LABEL = str('OK_LABEL');
  cr.ui.dialogs.BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');

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
   * <hr> elements in cr.ui.Menu.
   * This is a workaround for crbug.com/689255. This member variable is just for
   * keeping explicit reference to decorated <hr>s to prevent GC from collecting
   * <hr> wrappers, and not used anywhere.
   * TODO(fukino): Remove this member variable once the root cause is fixed.
   * @private {!Array<!Element>}
   */
  this.separators_ = [].slice.call(document.querySelectorAll('cr-menu > hr'));

  /**
   * Error dialog.
   * @type {!ErrorDialog}
   * @const
   */
  this.errorDialog = new ErrorDialog(this.element);

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
   * Multi-profile share dialog.
   * @type {!MultiProfileShareDialog}
   * @const
   */
  this.multiProfileShareDialog = new MultiProfileShareDialog(this.element);

  /**
   * Default task picker.
   * @type {!cr.filebrowser.DefaultTaskDialog}
   * @const
   */
  this.defaultTaskPicker =
      new cr.filebrowser.DefaultTaskDialog(this.element);

  /**
   * Suggest apps dialog.
   * @type {!SuggestAppsDialog}
   * @const
   */
  this.suggestAppsDialog = new SuggestAppsDialog(
      providersModel, this.element, launchParam.suggestAppsDialogState);

  /**
   * Dialog for installing .deb files
   * @type {!cr.filebrowser.InstallLinuxPackageDialog}
   * @const
   */
  this.installLinuxPackageDialog =
      new cr.filebrowser.InstallLinuxPackageDialog(this.element);

  /**
   * The container element of the dialog.
   * @type {!HTMLElement}
   */
  this.dialogContainer =
      queryRequiredElement('.dialog-container', this.element);

  /**
   * Context menu for texts.
   * @type {!cr.ui.Menu}
   * @const
   */
  this.textContextMenu = util.queryDecoratedElement(
      '#text-context-menu', cr.ui.Menu);

  /**
   * Location line.
   * @type {LocationLine}
   */
  this.locationLine = null;

  /**
   * The toolbar which contains controls.
   * @type {!HTMLElement}
   * @const
   */
  this.toolbar = queryRequiredElement('.dialog-header', this.element);

  /**
   * The actionbar which contains buttons to perform actions on selected
   * file(s).
   * @type {!HTMLElement}
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
   * Search box.
   * @type {!SearchBox}
   * @const
   */
  this.searchBox = new SearchBox(
      queryRequiredElement('#search-box', this.element),
      queryRequiredElement('#search-button', this.element));

  /**
   * Empty folder UI.
   * @type {!EmptyFolder}
   * @const
   */
  this.emptyFolder = new EmptyFolder(
      queryRequiredElement('#empty-folder', this.element));

  /**
   * Toggle-view button.
   * @type {!Element}
   * @const
   */
  this.toggleViewButton = queryRequiredElement('#view-button', this.element);

  /**
   * The button to sort the file list.
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.sortButton = util.queryDecoratedElement(
      '#sort-button', cr.ui.MenuButton);

  /**
   * Ripple effect of sort button.
   * @type {!FilesToggleRipple}
   * @const
   */
  this.sortButtonToggleRipple =
      /** @type {!FilesToggleRipple} */ (queryRequiredElement(
          'files-toggle-ripple', this.sortButton));

  /**
   * The button to open gear menu.
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.gearButton = util.queryDecoratedElement(
      '#gear-button', cr.ui.MenuButton);

  /**
   * The button to add new service (file system providers).
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.newServiceButton =
      util.queryDecoratedElement('#new-service-button', cr.ui.MenuButton);

  /**
   * Ripple effect of gear button.
   * @type {!FilesToggleRipple}
   * @const
   */
  this.gearButtonToggleRipple =
      /** @type {!FilesToggleRipple} */ (queryRequiredElement(
          'files-toggle-ripple', this.gearButton));

  /**
   * @type {!GearMenu}
   * @const
   */
  this.gearMenu = new GearMenu(this.gearButton.menu);

  /**
   * The button to open context menu in the check-select mode.
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.selectionMenuButton =
      util.queryDecoratedElement('#selection-menu-button', cr.ui.MenuButton);

  /**
   * Directory tree.
   * @type {DirectoryTree}
   */
  this.directoryTree = null;

  /**
   * Progress center panel.
   * @type {!ProgressCenterPanel}
   * @const
   */
  this.progressCenterPanel = new ProgressCenterPanel(
      queryRequiredElement('#progress-center', this.element));

  /**
   * List container.
   * @type {ListContainer}
   */
  this.listContainer = null;

  /**
   * @type {!HTMLElement}
   */
  this.formatPanelError =
      queryRequiredElement('#format-panel > .error', this.element);

  /**
   * @type {!cr.ui.Menu}
   * @const
   */
  this.fileContextMenu = util.queryDecoratedElement(
      '#file-context-menu', cr.ui.Menu);

  /**
   * @type {!HTMLMenuItemElement}
   * @const
   */
  this.fileContextMenu.defaultTaskMenuItem =
      /** @type {!HTMLMenuItemElement} */
      (queryRequiredElement('#default-task-menu-item', this.fileContextMenu));

  /**
   * @const {!cr.ui.MenuItem}
   */
  this.fileContextMenu.tasksSeparator = /** @type {!cr.ui.MenuItem} */
      (queryRequiredElement('#tasks-separator', this.fileContextMenu));

  /**
   * The combo button to specify the task.
   * @type {!cr.ui.ComboButton}
   * @const
   */
  this.taskMenuButton = util.queryDecoratedElement(
      '#tasks', cr.ui.ComboButton);
  this.taskMenuButton.showMenu = function(shouldSetFocus) {
    // Prevent the empty menu from opening.
    if (!this.menu.length)
      return;
    cr.ui.ComboButton.prototype.showMenu.call(this, shouldSetFocus);
  };

  /**
   * The menu button for share options
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.shareMenuButton =
      util.queryDecoratedElement('#share-menu-button', cr.ui.MenuButton);
  var shareMenuButtonToggleRipple =
      /** @type {!FilesToggleRipple} */ (
          queryRequiredElement('files-toggle-ripple', this.shareMenuButton));
  this.shareMenuButton.addEventListener('menushow', function() {
    shareMenuButtonToggleRipple.activated = true;
  });
  this.shareMenuButton.addEventListener('menuhide', function() {
    shareMenuButtonToggleRipple.activated = false;
  });

  /**
   * Banners in the file list.
   * @type {Banners}
   */
  this.banners = null;

  /**
   * Dialog footer.
   * @type {!DialogFooter}
   */
  this.dialogFooter = DialogFooter.findDialogFooter(
      this.dialogType_, /** @type {!Document} */ (this.element.ownerDocument));

  /**
   * @public {!ProvidersMenu}
   * @const
   */
  this.providersMenu = new ProvidersMenu(providersModel,
      util.queryDecoratedElement('#add-new-services-menu', cr.ui.Menu));

  /**
   * @public {!ActionsSubmenu}
   * @const
   */
  this.actionsSubmenu = new ActionsSubmenu(this.fileContextMenu);

  // Initialize attributes.
  this.element.setAttribute('type', this.dialogType_);

  // Hack: make menuitems focusable. Since the menuitems in the Files app is not
  // button so it doesn't have a tabfocus in nature. It prevents Chromevox from
  // speeaching because the opened menu is closed when the non-focusable object
  // tries to get the focus.
  var menuitems = document.querySelectorAll('cr-menu.chrome-menu > :not(hr)');
  for (var i = 0; i < menuitems.length; i++) {
    // Make menuitems focusable. The value can be any non-negative value,
    // because pressing 'Tab' key on menu is handled and we don't need to mind
    // the taborder and the destination of tabfocus.
    if (!menuitems[i].hasAttribute('tabindex'))
      menuitems[i].setAttribute('tabindex', '0');
  }

  // Modify UI default behavior.
  this.element.addEventListener('click', this.onExternalLinkClick_.bind(this));
  this.element.addEventListener('drop', function(e) {
    e.preventDefault();
  });
  if (util.runningInBrowser()) {
    this.element.addEventListener('contextmenu', function(e) {
      e.preventDefault();
      e.stopPropagation();
    });
  }
}

/**
 * Initializes here elements, which are expensive or hidden in the beginning.
 *
 * @param {!FileTable} table
 * @param {!FileGrid} grid
 * @param {!LocationLine} locationLine
 */
FileManagerUI.prototype.initAdditionalUI = function(table, grid, locationLine) {
  // List container.
  this.listContainer = new ListContainer(
      queryRequiredElement('#list-container', this.element), table, grid);

  // Splitter.
  this.decorateSplitter_(
      queryRequiredElement('#navigation-list-splitter', this.element));

  // Location line.
  this.locationLine = locationLine;

  // Init context menus.
  cr.ui.contextMenuHandler.setContextMenu(grid, this.fileContextMenu);
  cr.ui.contextMenuHandler.setContextMenu(table.list, this.fileContextMenu);
  cr.ui.contextMenuHandler.setContextMenu(
      queryRequiredElement('.drive-welcome.page'),
      this.fileContextMenu);

  // Add handlers.
  document.defaultView.addEventListener('resize', this.relayout.bind(this));
};

/**
 * Initializes the focus.
 */
FileManagerUI.prototype.initUIFocus = function() {
  // Set the initial focus. When there is no focus, the active element is the
  // <body>.
  var targetElement = null;
  if (this.dialogType_ == DialogType.SELECT_SAVEAS_FILE) {
    targetElement = this.dialogFooter.filenameInput;
  } else if (this.listContainer.currentListType !=
             ListContainer.ListType.UNINITIALIZED) {
    targetElement = this.listContainer.currentList;
  }

  if (targetElement)
    targetElement.focus();
};

/**
 * TODO(hirono): Merge the method into initAdditionalUI.
 * @param {!DirectoryTree} directoryTree
 */
FileManagerUI.prototype.initDirectoryTree = function(directoryTree) {
  this.directoryTree = directoryTree;

  // Set up the context menu for the volume/shortcut items in directory tree.
  this.directoryTree.contextMenuForRootItems =
      util.queryDecoratedElement('#roots-context-menu', cr.ui.Menu);
  this.directoryTree.contextMenuForSubitems =
      util.queryDecoratedElement('#directory-tree-context-menu', cr.ui.Menu);

  // Visible height of the directory tree depends on the size of progress
  // center panel. When the size of progress center panel changes, directory
  // tree has to be notified to adjust its components (e.g. progress bar).
  var relayoutLimiter = new AsyncUtil.RateLimiter(
      directoryTree.relayout.bind(directoryTree), 200);
  var observer = new MutationObserver(
      relayoutLimiter.run.bind(relayoutLimiter));
  observer.observe(this.progressCenterPanel.element,
                   /** @type {MutationObserverInit} */
                   ({subtree: true, attributes: true, childList: true}));
};

/**
 * TODO(mtomasz): Merge the method into initAdditionalUI if possible.
 * @param {!Banners} banners
 */
FileManagerUI.prototype.initBanners = function(banners) {
  this.banners = banners;
  this.banners.addEventListener('relayout', this.relayout.bind(this));
};

/**
 * Attaches files tooltip.
 */
FileManagerUI.prototype.attachFilesTooltip = function() {
  assertInstanceof(document.querySelector('files-tooltip'), FilesTooltip)
      .addTargets(document.querySelectorAll('[has-tooltip]'));
};

/**
 * Initialize files menu items. This method must be called after all files menu
 * items are decorated as cr.ui.MenuItem.
 */
FileManagerUI.prototype.decorateFilesMenuItems = function() {
  var filesMenuItems = document.querySelectorAll(
      'cr-menu.files-menu > cr-menu-item');

  for (var i = 0; i < filesMenuItems.length; i++) {
    var filesMenuItem = filesMenuItems[i];
    assertInstanceof(filesMenuItem, cr.ui.MenuItem);
    cr.ui.decorate(filesMenuItem, cr.ui.FilesMenuItem);
  }
};

/**
 * Relayouts the UI.
 */
FileManagerUI.prototype.relayout = function() {
  this.locationLine.truncate();
  // May not be available during initialization.
  if (this.listContainer.currentListType !==
      ListContainer.ListType.UNINITIALIZED) {
    this.listContainer.currentView.relayout();
  }
  if (this.directoryTree)
    this.directoryTree.relayout();
};

/**
 * Sets the current list type.
 * @param {ListContainer.ListType} listType New list type.
 */
FileManagerUI.prototype.setCurrentListType = function(listType) {
  this.listContainer.setCurrentListType(listType);

  var isListView = (listType === ListContainer.ListType.DETAIL);
  this.toggleViewButton.classList.toggle('thumbnail', isListView);

  var label = isListView ? str('CHANGE_TO_THUMBNAILVIEW_BUTTON_LABEL') :
                           str('CHANGE_TO_LISTVIEW_BUTTON_LABEL');
  this.toggleViewButton.setAttribute('aria-label', label);
  this.relayout();
};

/**
 * Overrides default handling for clicks on hyperlinks.
 * In a packaged apps links with targer='_blank' open in a new tab by
 * default, other links do not open at all.
 *
 * @param {!Event} event Click event.
 * @private
 */
FileManagerUI.prototype.onExternalLinkClick_ = function(event) {
  if (event.target.tagName != 'A' || !event.target.href)
    return;

  if (this.dialogType_ != DialogType.FULL_PAGE)
    this.dialogFooter.cancelButton.click();
};

/**
 * Decorates the given splitter element.
 * @param {!HTMLElement} splitterElement
 * @param {boolean=} opt_resizeNextElement
 * @private
 */
FileManagerUI.prototype.decorateSplitter_ = function(splitterElement,
    opt_resizeNextElement) {
  var self = this;
  var Splitter = cr.ui.Splitter;
  var customSplitter = cr.ui.define('div');

  customSplitter.prototype = {
    __proto__: Splitter.prototype,

    handleSplitterDragStart: function(e) {
      Splitter.prototype.handleSplitterDragStart.apply(this, arguments);
      this.ownerDocument.documentElement.classList.add('col-resize');
    },

    handleSplitterDragMove: function(deltaX) {
      Splitter.prototype.handleSplitterDragMove.apply(this, arguments);
      self.relayout();
    },

    handleSplitterDragEnd: function(e) {
      Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
      this.ownerDocument.documentElement.classList.remove('col-resize');
    }
  };

  /** @type Object */ (customSplitter).decorate(splitterElement);
  splitterElement.resizeNextElement = !!opt_resizeNextElement;
};

/**
 * Mark |element| with "loaded" attribute to indicate that File Manager has
 * finished loading.
 */
FileManagerUI.prototype.addLoadedAttribute = function() {
  this.element.setAttribute('loaded', '');
};

/**
 * Sets up and shows the alert to inform a user the task is opened in the
 * desktop of the running profile.
 *
 * @param {Array<Entry>} entries List of opened entries.
 */
FileManagerUI.prototype.showOpenInOtherDesktopAlert = function(entries) {
  if (!entries.length)
    return;
  chrome.fileManagerPrivate.getProfiles(
    function(profiles, currentId, displayedId) {
      // Find strings.
      var displayName;
      for (var i = 0; i < profiles.length; i++) {
        if (profiles[i].profileId === currentId) {
          displayName = profiles[i].displayName;
          break;
        }
      }
      if (!displayName) {
        console.warn('Display name is not found.');
        return;
      }

      var title = entries.length > 1 ?
          entries[0].name + '\u2026' /* ellipsis */ : entries[0].name;
      var message = strf(entries.length > 1 ?
                         'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                         'OPEN_IN_OTHER_DESKTOP_MESSAGE',
                         displayName,
                         currentId);

      // Show the dialog.
      this.alertDialog.showWithTitle(title, message, null, null, null);
    }.bind(this));
};

/**
 * Shows confirmation dialog and handles user interaction.
 * @param {boolean} isMove true if the operation is move. false if copy.
 * @param {!Array<string>} messages The messages to show in the dialog.
 *     box.
 * @return {!Promise<boolean>}
 */
FileManagerUI.prototype.showConfirmationDialog = function(isMove, messages) {
  var dialog = null;
  if (isMove) {
    dialog = this.moveConfirmDialog;
  } else {
    dialog = this.copyConfirmDialog;
  }
  return new Promise(function(resolve, reject) {
    dialog.show(
        messages.join(' '),
        function() {
          resolve(true);
        },
        function() {
          resolve(false);
        });
  });
};

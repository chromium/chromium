// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for utility functions.
 */
const filelist = {};

/**
 * File table list.
 */
class FileTableList extends cr.ui.table.TableList {
  constructor() {
    // To silence closure compiler.
    super();
    /*
     * @type {?function(number, number)}
     */
    this.onMergeItems_ = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * @param {function(number, number)} onMergeItems callback called from
   *     |mergeItems| with the parameters |beginIndex| and |endIndex|.
   */
  setOnMergeItems(onMergeItems) {
    assert(!this.onMergeItems_);
    this.onMergeItems_ = onMergeItems;
  }

  /** @override */
  mergeItems(beginIndex, endIndex) {
    super.mergeItems(beginIndex, endIndex);

    // Make sure that list item's selected attribute is updated just after the
    // mergeItems operation is done. This prevents checkmarks on selected items
    // from being animated unintentionally by redraw.
    for (let i = beginIndex; i < endIndex; i++) {
      const item = this.getListItemByIndex(i);
      if (!item) {
        continue;
      }
      const isSelected = this.selectionModel.getIndexSelected(i);
      if (item.selected !== isSelected) {
        item.selected = isSelected;
      }
    }

    if (this.onMergeItems_) {
      this.onMergeItems_(beginIndex, endIndex);
    }
  }

  /** @override */
  createSelectionController(sm) {
    return new FileListSelectionController(assert(sm), this);
  }

  /** @return {A11yAnnounce} */
  get a11y() {
    return this.table.a11y;
  }

  /**
   * @param {number} index Index of the list item.
   * @return {string}
   */
  getItemLabel(index) {
    return this.table.getItemLabel(index);
  }
}

/**
 * Decorates TableList as FileTableList.
 * @param {!cr.ui.table.TableList} self A tabel list element.
 */
FileTableList.decorate = self => {
  self.__proto__ = FileTableList.prototype;
  self.setAttribute('aria-multiselectable', true);
  /** @type {FileTableList} */ (self).onMergeItems_ = null;
};

/**
 * Selection controller for the file table list.
 */
class FileListSelectionController extends cr.ui.ListSelectionController {
  /**
   * @param {!cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {!FileTableList} tableList
   */
  constructor(selectionModel, tableList) {
    super(selectionModel);

    /** @const @private {!FileTapHandler} */
    this.tapHandler_ = new FileTapHandler();

    /** @const @private {!FileTableList} */
    this.tableList_ = tableList;
  }

  /** @override */
  handlePointerDownUp(e, index) {
    filelist.handlePointerDownUp.call(this, e, index);
  }

  /** @override */
  handleTouchEvents(e, index) {
    if (this.tapHandler_.handleTouchEvents(
            e, index, filelist.handleTap.bind(this))) {
      // If a tap event is processed, FileTapHandler cancels the event to
      // prevent triggering click events. Then it results not moving the focus
      // to the list. So we do that here explicitly.
      filelist.focusParentList(e);
    }
  }

  /** @override */
  handleKeyDown(e) {
    filelist.handleKeyDown.call(this, e);
  }

  /** @return {!FileTableList} */
  get filesView() {
    return this.tableList_;
  }
}

/**
 * Common item decoration for table's and grid's items.
 * @param {cr.ui.ListItem} li List item.
 * @param {Entry|FilesAppEntry} entry The entry.
 * @param {!MetadataModel} metadataModel Cache to
 *     retrieve metadada.
 */
filelist.decorateListItem = (li, entry, metadataModel) => {
  li.classList.add(entry.isDirectory ? 'directory' : 'file');
  // The metadata may not yet be ready. In that case, the list item will be
  // updated when the metadata is ready via updateListItemsMetadata. For files
  // not on an external backend, externalProps is not available.
  const externalProps = metadataModel.getCache([entry], [
    'hosted', 'availableOffline', 'customIconUrl', 'shared', 'isMachineRoot',
    'isExternalMedia'
  ])[0];
  filelist.updateListItemExternalProps(
      li, externalProps, util.isTeamDriveRoot(entry));

  // Overriding the default role 'list' to 'listbox' for better
  // accessibility on ChromeOS.
  li.setAttribute('role', 'option');
  li.setAttribute('aria-describedby', 'more-actions-info');

  Object.defineProperty(li, 'selected', {
    /**
     * @this {cr.ui.ListItem}
     * @return {boolean} True if the list item is selected.
     */
    get: function() {
      return this.hasAttribute('selected');
    },

    /**
     * @this {cr.ui.ListItem}
     */
    set: function(v) {
      if (v) {
        this.setAttribute('selected', '');
      } else {
        this.removeAttribute('selected');
      }
    }
  });
};

/**
 * Render the type column of the detail table.
 * @param {!Document} doc Owner document.
 * @param {!Entry} entry The Entry object to render.
 * @param {EntryLocation} locationInfo
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {!HTMLDivElement} Created element.
 */
filelist.renderFileTypeIcon = (doc, entry, locationInfo, opt_mimeType) => {
  const icon = /** @type {!HTMLDivElement} */ (doc.createElement('div'));
  icon.className = 'detail-icon';
  icon.setAttribute(
      'file-type-icon',
      FileType.getIcon(entry, opt_mimeType, locationInfo.rootType));
  return icon;
};

/**
 * Render filename label for grid and list view.
 * @param {!Document} doc Owner document.
 * @param {!Entry|!FilesAppEntry} entry The Entry object to render.
 * @param {EntryLocation} locationInfo
 * @return {!HTMLDivElement} The label.
 */
filelist.renderFileNameLabel = (doc, entry, locationInfo) => {
  // Filename need to be in a '.filename-label' container for correct
  // work of inplace renaming.
  const box = /** @type {!HTMLDivElement} */ (doc.createElement('div'));
  box.className = 'filename-label';
  const fileName = doc.createElement('span');
  fileName.className = 'entry-name';
  fileName.textContent = util.getEntryLabel(locationInfo, entry);
  box.appendChild(fileName);

  return box;
};

/**
 * Updates grid item or table row for the externalProps.
 * @param {cr.ui.ListItem} li List item.
 * @param {Object} externalProps Metadata.
 */
filelist.updateListItemExternalProps = (li, externalProps, isTeamDriveRoot) => {
  if (li.classList.contains('file')) {
    if (externalProps.availableOffline === false) {
      li.classList.add('dim-offline');
    } else {
      li.classList.remove('dim-offline');
    }
    // TODO(mtomasz): Consider adding some vidual indication for files which
    // are not cached on LTE. Currently we show them as normal files.
    // crbug.com/246611.

    if (externalProps.hosted === true) {
      li.classList.add('dim-hosted');
    } else {
      li.classList.remove('dim-hosted');
    }
  }

  const iconDiv = li.querySelector('.detail-icon');
  if (!iconDiv) {
    return;
  }

  if (externalProps.customIconUrl) {
    iconDiv.style.backgroundImage = 'url(' + externalProps.customIconUrl + ')';
  } else {
    iconDiv.style.backgroundImage = '';  // Back to the default image.
  }

  if (li.classList.contains('directory')) {
    iconDiv.classList.toggle('shared', !!externalProps.shared);
    iconDiv.classList.toggle('team-drive-root', !!isTeamDriveRoot);
    iconDiv.classList.toggle('computers-root', !!externalProps.isMachineRoot);
    iconDiv.classList.toggle(
        'external-media-root', !!externalProps.isExternalMedia);
  }
};

/**
 * Handles tap events on file list to change the selection state.
 *
 * @param {!Event} e The browser mouse event.
 * @param {number} index The index that was under the mouse pointer, -1 if
 *     none.
 * @param {!FileTapHandler.TapEvent} eventType
 * @return True if conducted any action. False when if did nothing special for
 *     tap.
 * @this {cr.ui.ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handleTap = function(e, index, eventType) {
  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);

  if (eventType === FileTapHandler.TapEvent.TWO_FINGER_TAP) {
    // Prepare to open the context menu in the same manner as the right click.
    // If the target is any of the selected files, open a one for those files.
    // If the target is a non-selected file, cancel current selection and open
    // context menu for the single file.
    // Otherwise (when the target is the background), for the current folder.
    if (index === -1) {
      // Two-finger tap outside the list should be handled here because it does
      // not produce mousedown/click events.
      this.filesView.a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
      sm.unselectAll();
    } else {
      const indexSelected = sm.getIndexSelected(index);
      if (!indexSelected) {
        // Prepare to open context menu of the new item by selecting only it.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.unselectAll();
        }
        sm.beginChange();
        sm.selectedIndex = index;
        sm.endChange();
        const name = this.filesView.getItemLabel(index);
        this.filesView.a11y.speakA11yMessage(
            strf('SELECTION_SINGLE_ENTRY', name));
      }
    }

    // Context menu will be opened for the selected files by the following
    // 'contextmenu' event.
    return false;
  }

  if (index === -1) {
    return false;
  }

  // Single finger tap.
  const isTap = eventType === FileTapHandler.TapEvent.TAP ||
      eventType === FileTapHandler.TapEvent.LONG_TAP;
  // Revert to click handling for single tap on checkbox or tap during rename.
  // Single tap on the checkbox in the list view mode should toggle select.
  // Single tap on input for rename should focus on input.
  const isCheckbox = e.target.classList.contains('detail-checkmark');
  const isRename = e.target.localName === 'input';
  if (eventType === FileTapHandler.TapEvent.TAP && (isCheckbox || isRename)) {
    return false;
  }

  if (sm.multiple && sm.getCheckSelectMode() && isTap && !e.shiftKey) {
    // toggle item selection. Equivalent to mouse click on checkbox.
    sm.beginChange();

    const name = this.filesView.getItemLabel(index);
    const msgId = sm.getIndexSelected(index) ? 'SELECTION_ADD_SINGLE_ENTRY' :
                                               'SELECTION_REMOVE_SINGLE_ENTRY';
    this.filesView.a11y.speakA11yMessage(strf(msgId, name));

    sm.setIndexSelected(index, !sm.getIndexSelected(index));
    // Toggle the current one and make it anchor index.
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
  } else if (
      sm.multiple && (eventType === FileTapHandler.TapEvent.LONG_PRESS)) {
    sm.beginChange();
    if (!sm.getCheckSelectMode()) {
      // Make sure to unselect the leading item that was not the touch target.
      sm.unselectAll();
      sm.setCheckSelectMode(true);
    }
    const name = this.filesView.getItemLabel(index);
    this.filesView.a11y.speakA11yMessage(
        strf('SELECTION_ADD_SINGLE_ENTRY', name));
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
    // Do not toggle selection yet, so as to avoid unselecting before drag.
  } else if (
      eventType === FileTapHandler.TapEvent.TAP && !sm.getCheckSelectMode()) {
    // Single tap should open the item with default action.
    // Select the item, so that MainWindowComponent will execute action of it.
    sm.beginChange();
    sm.unselectAll();
    const name = this.filesView.getItemLabel(index);
    this.filesView.a11y.speakA11yMessage(strf('SELECTION_SINGLE_ENTRY', name));
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
  }
  return false;
};

/**
 * Handles mouseup/mousedown events on file list to change the selection state.
 *
 * Basically the content of this function is identical to
 * cr.ui.ListSelectionController's handlePointerDownUp(), but following
 * handlings are inserted to control the check-select mode.
 *
 * 1) When checkmark area is clicked, toggle item selection and enable the
 *    check-select mode.
 * 2) When non-checkmark area is clicked in check-select mode, disable the
 *    check-select mode.
 *
 * @param {!Event} e The browser mouse event.
 * @param {number} index The index that was under the mouse pointer, -1 if
 *     none.
 * @this {cr.ui.ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handlePointerDownUp = function(e, index) {
  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);

  const anchorIndex = sm.anchorIndex;
  const isDown = (e.type === 'mousedown');

  const isTargetCheckmark = e.target.classList.contains('detail-checkmark') ||
      e.target.classList.contains('checkmark');
  // If multiple selection is allowed and the checkmark is clicked without
  // modifiers(Ctrl/Shift), the click should toggle the item's selection.
  // (i.e. same behavior as Ctrl+Click)
  const isClickOnCheckmark =
      (isTargetCheckmark && sm.multiple && index !== -1 && !e.shiftKey &&
       !e.ctrlKey && e.button === 0);

  sm.beginChange();

  if (index === -1) {
    this.filesView.a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.leadIndex = sm.anchorIndex = -1;
    sm.unselectAll();
  } else {
    if (sm.multiple && (e.ctrlKey || isClickOnCheckmark) && !e.shiftKey) {
      // Selection is handled at mouseUp.
      if (!isDown) {
        // 1) When checkmark area is clicked, toggle item selection and enable
        //    the check-select mode.
        if (isClickOnCheckmark) {
          // If Files app enters check-select mode by clicking an item's icon,
          // existing selection should be cleared.
          if (!sm.getCheckSelectMode()) {
            sm.unselectAll();
          }
        }
        // Always enables check-select mode when the selection is updated by
        // Ctrl+Click or Click on an item's icon.
        sm.setCheckSelectMode(true);

        // Toggle the current one and make it anchor index.
        const name = this.filesView.getItemLabel(index);
        const msgId = sm.getIndexSelected(index) ?
            'SELECTION_REMOVE_SINGLE_ENTRY' :
            'SELECTION_ADD_SINGLE_ENTRY';
        this.filesView.a11y.speakA11yMessage(strf(msgId, name));
        sm.setIndexSelected(index, !sm.getIndexSelected(index));
        sm.leadIndex = index;
        sm.anchorIndex = index;
      }
    } else if (e.shiftKey && anchorIndex !== -1 && anchorIndex !== index) {
      // Shift is done in mousedown.
      if (isDown) {
        sm.unselectAll();
        sm.leadIndex = index;
        if (sm.multiple) {
          sm.selectRange(anchorIndex, index);
          const nameStart = this.filesView.getItemLabel(anchorIndex);
          const nameEnd = this.filesView.getItemLabel(index);
          const count = Math.abs(index - anchorIndex) + 1;
          const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
          this.filesView.a11y.speakA11yMessage(msg);
        } else {
          const name = this.filesView.getItemLabel(index);
          this.filesView.a11y.speakA11yMessage(
              strf('SELECTION_SINGLE_ENTRY', name));
          sm.setIndexSelected(index, true);
        }
      }
    } else {
      // Right click for a context menu needs to not clear the selection.
      const isRightClick = e.button === 2;

      // If the index is selected this is handled in mouseup.
      const indexSelected = sm.getIndexSelected(index);
      if ((indexSelected && !isDown || !indexSelected && isDown) &&
          !(indexSelected && isRightClick)) {
        // 2) When non-checkmark area is clicked in check-select mode, disable
        //    the check-select mode.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.endChange();
          sm.unselectAll();
          sm.beginChange();
        }
        // This event handler is called for mouseup and mousedown, let's
        // announce the selection only in one of them.
        if (isDown) {
          const name = this.filesView.getItemLabel(index);
          this.filesView.a11y.speakA11yMessage(
              strf('SELECTION_SINGLE_ENTRY', name));
        }
        sm.selectedIndex = index;
      }
    }
  }
  sm.endChange();
};

/**
 * Handles key events on file list to change the selection state.
 *
 * Basically the content of this function is identical to
 * cr.ui.ListSelectionController's handleKeyDown(), but following handlings is
 * inserted to control the check-select mode.
 *
 * 1) When pressing direction key results in a single selection, the
 *    check-select mode should be terminated.
 *
 * @param {Event} e The keydown event.
 * @this {cr.ui.ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handleKeyDown = function(e) {
  const tagName = e.target.tagName;

  // If focus is in an input field of some kind, only handle navigation keys
  // that aren't likely to conflict with input interaction (e.g., text
  // editing, or changing the value of a checkbox or select).
  if (tagName === 'INPUT') {
    const inputType = e.target.type;
    // Just protect space (for toggling) for checkbox and radio.
    if (inputType === 'checkbox' || inputType === 'radio') {
      if (e.key === ' ') {
        return;
      }
      // Protect all but the most basic navigation commands in anything else.
    } else if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') {
      return;
    }
  }
  // Similarly, don't interfere with select element handling.
  if (tagName === 'SELECT') {
    return;
  }

  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);
  let newIndex = -1;
  const leadIndex = sm.leadIndex;
  let prevent = true;

  // Ctrl/Meta+A. Use keyCode=65 to use the same shortcut key regardless of
  // keyboard layout.
  const pressedKeyA = e.keyCode === 65 || e.key === 'a';
  if (sm.multiple && pressedKeyA &&
      (cr.isMac && e.metaKey || !cr.isMac && e.ctrlKey)) {
    this.filesView.a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
    sm.setCheckSelectMode(true);
    sm.selectAll();
    e.preventDefault();
    return;
  }

  // Esc
  if (e.key === 'Escape' && !e.ctrlKey && !e.shiftKey) {
    this.filesView.a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.unselectAll();
    e.preventDefault();
    return;
  }

  // Space: Note ChromeOS and ChromeOS on Linux can generate KeyDown Space
  // events differently the |key| attribute might be set to 'Unidentified'.
  if (e.code === 'Space' || e.key === ' ') {
    if (leadIndex !== -1) {
      const selected = sm.getIndexSelected(leadIndex);
      if (e.ctrlKey) {
        sm.beginChange();

        // Force selecting if it's the first item selected, otherwise flip the
        // "selected" status.
        if (selected && sm.selectedIndexes.length === 1 &&
            !sm.getCheckSelectMode()) {
          // It needs to go back/forth to trigger the 'change' event.
          sm.setIndexSelected(leadIndex, false);
          sm.setIndexSelected(leadIndex, true);
          const name = this.filesView.getItemLabel(leadIndex);
          this.filesView.a11y.speakA11yMessage(
              strf('SELECTION_SINGLE_ENTRY', name));
        } else {
          // Toggle the current one and make it anchor index.
          sm.setIndexSelected(leadIndex, !selected);
          const name = this.filesView.getItemLabel(leadIndex);
          const msgId = selected ? 'SELECTION_REMOVE_SINGLE_ENTRY' :
                                   'SELECTION_ADD_SINGLE_ENTRY';
          this.filesView.a11y.speakA11yMessage(strf(msgId, name));
        }

        // Force check-select, FileListSelectionModel.onChangeEvent_ resets it
        // if needed.
        sm.setCheckSelectMode(true);
        sm.endChange();

        // Prevents space to opening quickview.
        e.stopPropagation();
        e.preventDefault();
        return;
      }
    }
  }

  switch (e.key) {
    case 'Home':
      newIndex = this.getFirstIndex();
      break;
    case 'End':
      newIndex = this.getLastIndex();
      break;
    case 'ArrowUp':
      newIndex = leadIndex === -1 ? this.getLastIndex() :
                                    this.getIndexAbove(leadIndex);
      break;
    case 'ArrowDown':
      newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                    this.getIndexBelow(leadIndex);
      break;
    case 'ArrowLeft':
    case 'MediaTrackPrevious':
      newIndex = leadIndex === -1 ? this.getLastIndex() :
                                    this.getIndexBefore(leadIndex);
      break;
    case 'ArrowRight':
    case 'MediaTrackNext':
      newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                    this.getIndexAfter(leadIndex);
      break;
    default:
      prevent = false;
  }

  if (newIndex >= 0 && newIndex < sm.length) {
    sm.beginChange();

    sm.leadIndex = newIndex;
    if (e.shiftKey) {
      const anchorIndex = sm.anchorIndex;
      if (sm.multiple) {
        sm.unselectAll();
      }
      if (anchorIndex === -1) {
        const name = this.filesView.getItemLabel(newIndex);
        this.filesView.a11y.speakA11yMessage(
            strf('SELECTION_SINGLE_ENTRY', name));
        sm.setIndexSelected(newIndex, true);
        sm.anchorIndex = newIndex;
      } else {
        const nameStart = this.filesView.getItemLabel(anchorIndex);
        const nameEnd = this.filesView.getItemLabel(newIndex);
        const count = Math.abs(newIndex - anchorIndex) + 1;
        const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
        this.filesView.a11y.speakA11yMessage(msg);
        sm.selectRange(anchorIndex, newIndex);
      }
    } else if (e.ctrlKey) {
      // While Ctrl is being held, only leadIndex and anchorIndex are moved.
      sm.anchorIndex = newIndex;
    } else {
      // 1) When pressing direction key results in a single selection, the
      //    check-select mode should be terminated.
      sm.setCheckSelectMode(false);

      if (sm.multiple) {
        sm.unselectAll();
      }
      const name = this.filesView.getItemLabel(newIndex);
      this.filesView.a11y.speakA11yMessage(
          strf('SELECTION_SINGLE_ENTRY', name));
      sm.setIndexSelected(newIndex, true);
      sm.anchorIndex = newIndex;
    }

    sm.endChange();

    if (prevent) {
      e.preventDefault();
    }
  }
};

/**
 * Focus on the file list that contains the event target.
 * @param {!Event} event the touch event.
 */
filelist.focusParentList = event => {
  let element = event.target;
  while (element && !(element instanceof cr.ui.List)) {
    element = element.parentElement;
  }
  if (element) {
    element.focus();
  }
};

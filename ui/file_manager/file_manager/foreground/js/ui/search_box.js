// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../elements/files_toggle_ripple.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {dispatchSimpleEvent} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {ListItem} from 'chrome://resources/js/cr/ui/list_item.m.js';

import {FileType} from '../../../common/js/file_type.js';
import {metrics} from '../../../common/js/metrics.js';
import {strf, util} from '../../../common/js/util.js';

import {AutocompleteList} from './autocomplete_list.js';

/**
 * Search box.
 */
export class SearchBox extends EventTarget {
  /**
   * @param {!Element} element Root element of the search box.
   * @param {!Element} searchWrapper Wrapper element around the buttons and box.
   * @param {!Element} searchButton Search button.
   */
  constructor(element, searchWrapper, searchButton) {
    super();

    /**
     * Autocomplete List.
     * @type {!SearchBox.AutocompleteList}
     */
    this.autocompleteList =
        new SearchBox.AutocompleteList(element.ownerDocument);

    /**
     * Root element of the search box.
     * @type {!Element}
     */
    this.element = element;

    /**
     * Search wrapper.
     * @type {!Element}
     */
    this.searchWrapper = searchWrapper;

    /**
     * Search button.
     * @type {!Element}
     */
    this.searchButton = searchButton;

    /**
     * Ripple effect of search button.
     * @private {!FilesToggleRippleElement}
     * @const
     */
    this.searchButtonToggleRipple_ =
        /** @type {!FilesToggleRippleElement} */ (util.queryRequiredElement(
            'files-toggle-ripple', this.searchButton));

    /**
     * Text input of the search box.
     * @type {!HTMLInputElement}
     */
    this.inputElement =
        /** @type {!HTMLInputElement} */ (element.querySelector('cr-input'));

    /**
     * Clear button of the search box.
     * @private {!Element}
     */
    this.clearButton_ = assert(element.querySelector('.clear'));

    /** @private {boolean} */
    this.isClicking_ = false;

    this.collapsed = true;

    // Register events.
    this.inputElement.addEventListener('input', this.onInput_.bind(this));
    this.inputElement.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.inputElement.addEventListener('focus', this.onFocus_.bind(this));
    this.inputElement.addEventListener('blur', this.onBlur_.bind(this));
    this.inputElement.ownerDocument.addEventListener(
        'dragover', this.onDragEnter_.bind(this), true);
    this.inputElement.ownerDocument.addEventListener(
        'dragend', this.onDragEnd_.bind(this));
    this.searchButton.addEventListener(
        'click', this.onSearchButtonClick_.bind(this));
    this.clearButton_.addEventListener(
        'click', this.onClearButtonClick_.bind(this));
    const dispatchItemSelect = () => {
      dispatchSimpleEvent(this, SearchBox.EventType.ITEM_SELECT);
    };
    this.autocompleteList.handleEnterKeydown = dispatchItemSelect;
    this.autocompleteList.addEventListener('mousedown', dispatchItemSelect);

    document.addEventListener('mousedown', () => {
      if (this.collapsed) {
        return;
      }
      this.isClicking_ = true;
    }, {capture: true, passive: true});

    document.addEventListener('mouseup', () => {
      if (this.collapsed) {
        return;
      }
      this.isClicking_ = false;
      window.requestAnimationFrame(() => {
        this.removeHidePending();
      });
    }, {passive: true});

    this.searchWrapper.addEventListener(
        'focusout', this.onFocusOut_.bind(this));

    // Append dynamically created element.
    element.parentNode.appendChild(this.autocompleteList);
  }

  /** @return {boolean} */
  get collapsed() {
    return this.searchWrapper.hasAttribute('collapsed');
  }

  /**
   * @private
   * @param {boolean} collapsed
   */
  set collapsed(collapsed) {
    if (collapsed) {
      this.searchWrapper.setAttribute('collapsed', true);
    } else {
      this.searchWrapper.removeAttribute('collapsed');
    }
  }

  /**
   * Clears the search query.
   */
  clear() {
    this.inputElement.value = '';
    this.updateStyles_();
  }

  /**
   * Sets hidden attribute for components of search box.
   * @param {boolean} hidden True when the search box need to be hidden.
   */
  setHidden(hidden) {
    this.element.hidden = hidden;
    this.searchButton.hidden = hidden;
  }

  /**
   * Focus out event handler.
   * @private
   */
  onFocusOut_() {
    window.requestAnimationFrame(() => {
      // If the focus is still within the search box don't hide the input.
      if (document.activeElement &&
          this.element.contains(document.activeElement)) {
        return;
      }

      // If the focus is moved due to a user click, we don't collapse the searc
      // box here. We wait until "mouseup" to let the mouse events be processed
      // by the button user is clickinkg, which might change position due to the
      // search box collapse.
      if (this.isClicking_) {
        return;
      }

      if (this.element.classList.contains('hide-pending')) {
        this.removeHidePending();
      }
    });
  }

  /**
   * @private
   */
  onInput_() {
    this.updateStyles_();
    dispatchSimpleEvent(this, SearchBox.EventType.TEXT_CHANGE);
  }

  /**
   * Handles a focus event of the search box <cr-input> element.
   * @private
   */
  onFocus_() {
    // Early out if we closing the search cr-input: do not just go ahead and
    // re-open it on focus, crbug.com/668427.
    if (this.element.classList.contains('hide-pending')) {
      return;
    }

    this.inputElement.addEventListener('transitionend', () => {
      this.collapsed = false;
    }, {once: true});

    this.isClicking_ = false;
    this.element.classList.toggle('has-cursor', true);
    this.searchWrapper.classList.toggle('has-cursor', true);
    this.autocompleteList.attachToInput(this.inputElement);
    this.updateStyles_();
    this.searchButtonToggleRipple_.activated = true;
    metrics.recordUserAction('SelectSearch');
  }

  /**
   * Handles a blur event of the search box <cr-input> element.
   * @private
   */
  onBlur_() {
    this.element.classList.toggle('has-cursor', false);
    this.element.classList.toggle('hide-pending', true);
    this.searchWrapper.classList.toggle('has-cursor', false);
    this.searchWrapper.classList.toggle('hide-pending', true);
    this.autocompleteList.detach();
    this.updateStyles_();
    this.searchButtonToggleRipple_.activated = false;
  }

  /**
   * Handles delayed hiding of the search box (until click).
   */
  removeHidePending() {
    this.inputElement.disabled = this.inputElement.value.length == 0;
    this.element.classList.toggle('hide-pending', false);
    this.searchWrapper.classList.toggle('hide-pending', false);
    this.inputElement.addEventListener('transitionend', () => {
      this.collapsed = true;
    }, {once: true});
  }

  /**
   * Handles a keydown event of the search box.
   * @param {Event} event
   * @private
   */
  onKeyDown_(event) {
    event = /** @type {KeyboardEvent} */ (event);
    // Handle only Esc key now.
    if (event.key != 'Escape' || this.inputElement.value) {
      return;
    }

    this.inputElement.tabIndex = -1;  // Focus to default element after blur.
    this.inputElement.blur();
    this.inputElement.disabled = this.inputElement.value.length == 0;
    this.element.classList.toggle('hide-pending', false);
    this.searchWrapper.classList.toggle('hide-pending', false);
  }

  /**
   * Handles a dragenter event and refuses a drag source of files.
   * @param {Event} event The dragenter event.
   * @private
   */
  onDragEnter_(event) {
    event = /** @type {DragEvent} */ (event);
    // For normal elements, they does not accept drag drop by default, and
    // accept it by using event.preventDefault. But input elements accept drag
    // drop by default. So disable the input element here to prohibit drag drop.
    if (event.dataTransfer.types.indexOf('text/plain') === -1) {
      this.inputElement.style.pointerEvents = 'none';
    }
  }

  /**
   * Handles a dragend event.
   * @private
   */
  onDragEnd_() {
    this.inputElement.style.pointerEvents = '';
  }

  /**
   * Updates styles of the search box.
   * @private
   */
  updateStyles_() {
    const hasText = !!this.inputElement.value;
    this.element.classList.toggle('has-text', hasText);
    this.searchWrapper.classList.toggle('has-text', hasText);
    const hasFocusOnInput = this.element.classList.contains('has-cursor');

    // Focus either the search button or the input.
    this.inputElement.tabIndex = (hasText || hasFocusOnInput) ? 0 : -1;
    this.searchButton.tabIndex = (hasText || hasFocusOnInput) ? -1 : 0;
  }

  /**
   * @private
   */
  onSearchButtonClick_() {
    this.inputElement.disabled = false;
    this.inputElement.focus();
  }

  /**
   * @private
   */
  onClearButtonClick_() {
    this.inputElement.value = '';
    this.onInput_();
    // The search box will be collapsed after Clear, so the search button will
    // animate to a new position, we need to call focus() after the animation
    // to make sure the tooltip shows at the correct position.
    this.inputElement.addEventListener('transitionend', () => {
      this.searchButton.focus();
    }, {once: true});
  }
}

/**
 * Event type.
 * @enum {string}
 */
SearchBox.EventType = {
  // Dispatched when the text in the search box is changed.
  TEXT_CHANGE: 'textchange',
  // Dispatched when the item in the auto complete list is selected.
  ITEM_SELECT: 'itemselect',
};

/**
 * Autocomplete list for search box.
 */
SearchBox.AutocompleteList = class extends AutocompleteList {
  /**
   * @param {Document} document Document.
   */
  constructor(document) {
    super();
    this.__proto__ = SearchBox.AutocompleteList.prototype;
    this.id = 'autocomplete-list';
    this.autoExpands = true;
    this.itemConstructor = /** @type {function(new:ListItem, *)} */ (
        SearchBox.AutocompleteListItem_.bind(null, document));
    this.addEventListener('mouseover', this.onMouseOver_.bind(this));
  }

  /**
   * Do nothing when a suggestion is selected.
   * @override
   */
  handleSelectedSuggestion() {}

  /**
   * Change the selection by a mouse over instead of just changing the
   * color of moused over element with :hover in CSS. Here's why:
   *
   * 1) The user selects an item A with up/down keys (item A is highlighted)
   * 2) Then the user moves the cursor to another item B
   *
   * If we just change the color of moused over element (item B), both
   * the item A and B are highlighted. This is bad. We should change the
   * selection so only the item B is highlighted.
   *
   * @param {Event} event Event.
   * @private
   */
  onMouseOver_(event) {
    if (event.target.itemInfo) {
      this.selectedItem = event.target.itemInfo;
    }
  }
};

/**
 * ListItem element for autocomplete.
 * @private
 */
SearchBox.AutocompleteListItem_ = class AutocompleteListItem_ extends ListItem {
  /**
   * @param {Document} document Document.
   * @param {SearchItem|chrome.fileManagerPrivate.DriveMetadataSearchResult}
   *     item An object
   * representing a suggestion.
   */
  constructor(document, item) {
    super();
    this.itemInfo = item;

    const icon = document.createElement('div');
    icon.className = 'detail-icon';

    const text = document.createElement('div');
    text.className = 'detail-text';

    if (item.isHeaderItem) {
      icon.setAttribute('search-icon', '');
      text.innerHTML =
          strf('SEARCH_DRIVE_HTML', util.htmlEscape(item.searchQuery));
    } else {
      const iconType = FileType.getIcon(item.entry);
      icon.setAttribute('file-type-icon', iconType);
      // highlightedBaseName is a piece of HTML with meta characters properly
      // escaped. See the comment at fileManagerPrivate.searchDriveMetadata().
      text.innerHTML = item.highlightedBaseName;
    }
    this.appendChild(icon);
    this.appendChild(text);
  }
};

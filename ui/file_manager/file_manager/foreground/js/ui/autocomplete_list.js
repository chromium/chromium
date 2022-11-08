// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {AnchorType, positionPopupAroundElement} from './position_util.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';

import {List} from './list.js';
import {ListItem} from './list_item.js';
import {ListSingleSelectionModel} from './list_single_selection_model.js';


/**
 * Creates a new autocomplete list popup.
 * @extends {List}
 */
export class AutocompleteList extends List {
  constructor() {
    super();
    this.__proto__ = AutocompleteList.prototype;

    /**
     * The text field the autocomplete popup is currently attached to, if any.
     * @type {HTMLElement}
     * @private
     */
    this.targetInput_ = null;

    /**
     * Keydown event listener to attach to a text field.
     * @type {Function}
     * @private
     */
    this.textFieldKeyHandler_ = null;

    /**
     * Input event listener to attach to a text field.
     * @type {Function}
     * @private
     */
    this.textFieldInputHandler_ = null;

    /**
     * syncWidthAndPositionToInput function bound to |this|.
     * @type {!Function|undefined}
     * @private
     */
    this.boundSyncWidthAndPositionToInput_ = undefined;

    this.decorate();
  }

  /** @override */
  decorate() {
    List.prototype.decorate.call(this);

    this.classList.add('autocomplete-suggestions');
    this.selectionModel = new ListSingleSelectionModel();

    this.itemConstructor = ListItem;
    this.textFieldKeyHandler_ = this.handleAutocompleteKeydown_.bind(this);
    const self = this;
    this.textFieldInputHandler_ = function(e) {
      self.requestSuggestions(self.targetInput_.value);
    };
    this.addEventListener('change', function(e) {
      if (self.selectedItem) {
        self.handleSelectedSuggestion(self.selectedItem);
      }
    });
    // Start hidden; adding suggestions will unhide.
    this.hidden = true;
  }

  /** @override */
  createItem(pageInfo) {
    return new this.itemConstructor(pageInfo);
  }

  /**
   * The suggestions to show.
   * @type {Array}
   */
  set suggestions(suggestions) {
    this.dataModel = new ArrayDataModel(suggestions);
    this.hidden = !this.targetInput_ || suggestions.length == 0;
  }

  /**
   * Requests new suggestions. Called when new suggestions are needed.
   * @param {string} query the text to autocomplete from.
   */
  requestSuggestions(query) {}

  /**
   * Handles the Enter keydown event.
   * By default, clears and hides the autocomplete popup. Note that the
   * keydown event bubbles up, so the input field can handle the event.
   */
  handleEnterKeydown() {
    this.suggestions = [];
  }

  /**
   * Handles the selected suggestion. Called when a suggestion is selected.
   * By default, sets the target input element's value to the 'url' field
   * of the selected suggestion.
   * @param {Object} selectedSuggestion
   */
  handleSelectedSuggestion(selectedSuggestion) {
    const input = this.targetInput_;
    if (!input) {
      return;
    }
    input.value = selectedSuggestion['url'];
    // Programmatically change the value won't trigger a change event, but
    // clients are likely to want to know when changes happen, so fire one.
    dispatchSimpleEvent(input, 'change', true);
  }

  /**
   * Attaches the popup to the given input element. Requires
   * that the input be wrapped in a block-level container of the same width.
   * @param {HTMLElement} input The input element to attach to.
   */
  attachToInput(input) {
    if (this.targetInput_ == input) {
      return;
    }

    this.detach();
    this.targetInput_ = input;
    this.style.width = input.getBoundingClientRect().width + 'px';
    this.hidden = false;  // Necessary for positionPopupAroundElement to work.
    positionPopupAroundElement(input, this, AnchorType.BELOW);
    // Start hidden; when the data model gets results the list will show.
    this.hidden = true;

    input.addEventListener('keydown', this.textFieldKeyHandler_, true);
    input.addEventListener('input', this.textFieldInputHandler_);

    if (!this.boundSyncWidthAndPositionToInput_) {
      this.boundSyncWidthAndPositionToInput_ =
          this.syncWidthAndPositionToInput.bind(this);
    }
    // We need to call syncWidthAndPositionToInput whenever page zoom level or
    // page size is changed.
    window.addEventListener('resize', this.boundSyncWidthAndPositionToInput_);
  }

  /**
   * Detaches the autocomplete popup from its current input element, if any.
   */
  detach() {
    const input = this.targetInput_;
    if (!input) {
      return;
    }

    input.removeEventListener('keydown', this.textFieldKeyHandler_, true);
    input.removeEventListener('input', this.textFieldInputHandler_);
    this.targetInput_ = null;
    this.suggestions = [];
    if (this.boundSyncWidthAndPositionToInput_) {
      window.removeEventListener(
          'resize', this.boundSyncWidthAndPositionToInput_);
    }
  }

  /**
   * Makes sure that the suggestion list matches the width and the position
   * of the input it is attached to. Should be called any time the input is
   * resized.
   */
  syncWidthAndPositionToInput() {
    const input = this.targetInput_;
    if (input) {
      this.style.width = input.getBoundingClientRect().width + 'px';
      positionPopupAroundElement(input, this, AnchorType.BELOW);
    }
  }

  /**
   * @return {HTMLElement} The text field the autocomplete popup is currently
   *     attached to, if any.
   */
  get targetInput() {
    return this.targetInput_;
  }

  /**
   * Handles input field key events that should be interpreted as autocomplete
   * commands.
   * @param {Event} event The keydown event.
   * @private
   */
  handleAutocompleteKeydown_(event) {
    if (this.hidden) {
      return;
    }
    let handled = false;
    switch (event.key) {
      case 'Escape':
        this.suggestions = [];
        handled = true;
        break;
      case 'Enter':
        // If the user has already selected an item using the arrow keys then
        // presses Enter, keep |handled| = false, so the input field can
        // handle the event as well.
        this.handleEnterKeydown();
        break;
      case 'ArrowUp':
      case 'ArrowDown':
        this.dispatchEvent(new KeyboardEvent(event.type, event));
        handled = true;
        break;
    }
    // Don't let arrow keys affect the text field, or bubble up to, e.g.,
    // an enclosing list item.
    if (handled) {
      event.preventDefault();
      event.stopPropagation();
    }
  }
}
AutocompleteList.prototype.__proto__ = List.prototype;

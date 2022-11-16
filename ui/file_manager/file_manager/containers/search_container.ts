// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {queryRequiredElement} from '../common/js/dom_utils.js';
import {SearchAutocompleteList} from '../foreground/js/ui/search_autocomplete_list.js';

/**
 * @fileoverview
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

/**
 * Defines the possible states of the query input widget. This is a widget with
 * a search button, text input and clear button. By default, the widget is
 * closed. When active, it is open. Due to CSS transitions it has two
 * intermediate states, OPENING and CLOSING.
 */
enum SearchInputState {
  CLOSED = 'closed',
  OPENING = 'opening',
  OPEN = 'open',
  CLOSING = 'closing',
}

/**
 * The controller for the search UI elements. The controller takes care of the
 * behavior of UI elements. It must not deal with the look-and-feel. It finds
 * them, hooks to the UI events and drives the business logic based on those UI
 * events.
 */
export class SearchContainer extends EventTarget {
  // The button that clears the search query.
  private clearButton_: HTMLElement;
  // The element contains the the input element.
  private searchBox_: HTMLElement;
  // The input element where the user enters the query.
  private inputElement_: CrInputElement;
  // The search button that shows the query input element.
  private searchButton_: HTMLElement;
  // The div that holds elements all of the UI elements.
  private searchWrapper_: HTMLElement;
  // The current state of the search widget elements.
  private inputState_: SearchInputState = SearchInputState.CLOSED;
  // A component that shows potential matches for the text the user typed so
  // far.
  private autocompleteList_: SearchAutocompleteList;

  /**
   * Builds a search container that creates and manages UI elements. This
   * container receives a reference to a container, |searchWrapper| that
   * contains other UI elements. It uses the container to fetch them by IDs.
   * Once the UI elements are found, this container makes itself the listener of
   * the events that are posted to by the UI elements and converts them to
   * business logic. This includes notifying listeners when the the search query
   * changes or the auto-complete item is selected.
   */
  constructor(searchWrapper: HTMLElement) {
    super();

    // The "box" around the search button, query input, and clear button.
    this.searchBox_ =
        queryRequiredElement('#search-box', searchWrapper) as HTMLElement;

    // The button that opens and closes the query input element.
    this.searchButton_ =
        queryRequiredElement('#search-button', searchWrapper) as HTMLElement;

    // The element that holds search UI elements.
    this.searchWrapper_ = searchWrapper;
    // Start in the collapsed state. This attribute is read in tests.
    this.searchWrapper_.setAttribute('collapsed', '');

    // Text input element where the user enters the query.
    this.inputElement_ =
        this.searchBox_.querySelector('cr-input') as CrInputElement;

    // The button that allows the user to clear the query.
    this.clearButton_ = this.searchBox_.querySelector('.clear') as HTMLElement;

    // The list showing possible matches to the current query.
    this.autocompleteList_ =
        new SearchAutocompleteList(this.searchBox_.ownerDocument);
    this.searchWrapper_.parentNode!.appendChild(this.autocompleteList_);

    this.setupEventHandlers();
  }

  /**
   * Sets hidden attribute for components of search box.
   */
  setHidden(hidden: boolean) {
    if (hidden) {
      this.searchBox_.setAttribute('hidden', 'true');
      this.searchButton_.setAttribute('hidden', 'true');
    } else {
      this.searchBox_.removeAttribute('hidden');
      this.searchButton_.removeAttribute('hidden');
    }
  }

  /**
   * Clears the current search query. If the query changed as a result, it posts
   * a query changed event.
   */
  clear() {
    const value = this.inputElement_.value;
    if (value !== '') {
      this.inputElement_.value = '';
      this.postQueryChangedEvent();
      requestAnimationFrame(() => {
        this.closeSearch();
        this.searchButton_.focus();
      });
    }
  }

  /**
   * Sets the new query. This method does not post events, even if the query
   * changed as a result.
   */
  setQuery(query: string) {
    this.inputElement_.value = query;
    this.inputElement_.focus();
  }

  /**
   * Returns the user entered search query. This method trims white spaces from
   * the left side of the query.
   */
  getQuery(): string {
    return this.inputElement_.value.trimLeft();
  }

  /**
   * Clears all suggestion
   */
  clearSuggestions() {
    this.autocompleteList_.suggestions = [];
  }

  /**
   * Sets the first suggestion in the autocomplete list. This typically should
   * be a header item that indicates what is being searched and where.
   */
  setHeaderItem(item: SearchItem) {
    if (!this.autocompleteList_.dataModel ||
        this.autocompleteList_.dataModel.length == 0) {
      this.autocompleteList_.suggestions = [item];
    } else {
      // Updates only the head item to prevent a flickering on typing.
      this.autocompleteList_.dataModel.splice(0, 1, item);
    }
    this.autocompleteList_.syncWidthAndPositionToInput();
  }

  /**
   * Sets the given search items as autocomplete suggestion. This method
   * overrides all suggestions, so if you need to keep the header item, you
   * should pass it as the first item in the list.
   */
  setSuggestions(items: SearchItem[]) {
    this.autocompleteList_.suggestions = items;
    this.autocompleteList_.syncWidthAndPositionToInput();
  }

  /**
   * Returns the currently selected search item on the autocomplete list.
   */
  getSelectedItem(): SearchItem {
    return this.autocompleteList_.selectedItem;
  }

  /**
   * Attaches all necessary event listeners to the UI elements that make the
   * search interface. This method must be called as the last statement of the
   * constructor.
   */
  private setupEventHandlers() {
    this.searchButton_.addEventListener('click', () => {
      if (this.inputState_ === SearchInputState.CLOSED) {
        this.openSearch();
      } else if (this.inputState_ === SearchInputState.OPEN) {
        this.closeSearch();
      } else {
        console.warn('Search UI is transitioning', this.inputState_);
      }
    });
    this.inputElement_.addEventListener('input', () => {
      this.postQueryChangedEvent();
    });
    this.inputElement_.addEventListener('keydown', (event: KeyboardEvent) => {
      if (!this.inputElement_.value) {
        if (event.key === 'Escape') {
          this.closeSearch();
        }
        if (event.key === 'Tab') {
          this.closeSearch();
        }
      }
    });
    this.inputElement_.addEventListener('focus', () => {
      this.autocompleteList_.attachToInput(this.inputElement_);
    });
    this.inputElement_.addEventListener('blur', () => {
      this.autocompleteList_.detach();
    });
    this.clearButton_.addEventListener('click', () => {
      this.clear();
    });
    // Hide the search if the user clicks outside it and there is no search
    // query entered.
    document.addEventListener('click', (event) => {
      if (!this.inputElement_.value) {
        const target = event.target;
        if (target instanceof Node) {
          if (!this.searchWrapper_.contains(target as Node)) {
            if (this.inputState_ === SearchInputState.OPEN) {
              this.closeSearch();
            }
          }
        }
      }
    });
    this.autocompleteList_.handleEnterKeydown =
        this.postItemChangedEvent.bind(this);
    this.autocompleteList_.addEventListener(
        'mousedown', this.postItemChangedEvent.bind(this));
  }

  /**
   * Starts the process of opening the search widget. We use CSS transitions to
   * open the widget and thus the widget it not fully opened until the CSS
   * transition finishes.
   */
  private openSearch() {
    // Do not initiate open transition if we are not closed. This would leave us
    // in the OPENING state, without ever getting to OPEN state.
    if (this.inputState_ === SearchInputState.CLOSED) {
      this.inputState_ = SearchInputState.OPENING;
      this.inputElement_.disabled = false;
      this.inputElement_.tabIndex = 0;
      this.inputElement_.focus();
      this.inputElement_.addEventListener('transitionend', () => {
        this.inputState_ = SearchInputState.OPEN;
        this.searchWrapper_.removeAttribute('collapsed');
      }, {once: true, passive: true, capture: true});
      this.searchWrapper_.classList.add('has-cursor', 'has-text');
      this.searchBox_.classList.add('has-cursor', 'has-text');
    }
  }

  /**
   * Starts the process of closing the search widget. We use CSS transitions to
   * close the widget and thus the widget it not fully closed until the CSS
   * transition finishes.
   */
  private closeSearch() {
    // Do not initiate close transition if we are not open. This would leave us
    // in the CLOSING state, without ever getting to CLOSED state.
    if (this.inputState_ === SearchInputState.OPEN) {
      this.inputState_ = SearchInputState.CLOSING;
      this.inputElement_.tabIndex = -1;
      this.inputElement_.disabled = true;
      this.inputElement_.blur();
      this.inputElement_.addEventListener('transitionend', () => {
        this.inputState_ = SearchInputState.CLOSED;
        this.searchWrapper_.setAttribute('collapsed', '');
      }, {once: true, passive: true, capture: true});
      this.searchWrapper_.classList.remove('has-cursor', 'has-text');
      this.searchBox_.classList.remove('has-cursor', 'has-text');
    }
  }

  /**
   * Generates a custom event with the current value of the input element as the
   * search query.
   */
  private postQueryChangedEvent() {
    this.dispatchEvent(new CustomEvent(SEARCH_QUERY_CHANGED, {
      bubbles: true,
      composed: true,
      detail: {
        query: this.inputElement_.value,
      },
    }));
  }

  private postItemChangedEvent() {
    this.dispatchEvent(new CustomEvent(SEARCH_ITEM_CHANGED, {
      bubbles: true,
      composed: true,
      detail: {
        item: this.getSelectedItem(),
      },
    }));
  }
}

/**
 * The name of the event posted when the search query changes.
 */
export const SEARCH_QUERY_CHANGED = 'search-query-changed';

/**
 * The name of the event posted when a new autocomplete item is selected.
 */
export const SEARCH_ITEM_CHANGED = 'search-item-changed';

export interface QueryChange {
  query: string;
}

export interface ItemChange {
  item: SearchItem;
}

/**
 * A custom event that informs listeners that the query has changed.
 */
export type SearchQueryChangedEvent = CustomEvent<QueryChange>;

/**
 * A custom event that informs the listeners that an item in the autocomplete
 * list has been selected.
 */
export type SearchItemChangedEvent = CustomEvent<ItemChange>;

declare global {
  interface HTMLElementEventMap {
    [SEARCH_QUERY_CHANGED]: SearchQueryChangedEvent;
    [SEARCH_ITEM_CHANGED]: SearchItemChangedEvent;
  }
}

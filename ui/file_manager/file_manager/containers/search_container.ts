// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {queryRequiredElement} from '../common/js/dom_utils.js';
import {str, util} from '../common/js/util.js';
import {PropStatus, SearchData, SearchFileType, SearchLocation, SearchOptions, SearchRecency, State} from '../externs/ts/state.js';
import {VolumeManager} from '../externs/volume_manager.js';
import {PathComponent} from '../foreground/js/path_component.js';
import {SearchAutocompleteList} from '../foreground/js/ui/search_autocomplete_list.js';
import {clearSearch, updateSearch} from '../state/actions.js';
import {getDefaultSearchOptions, getStore, Store} from '../state/store.js';
import {XfPathDisplayElement} from '../widgets/xf_path_display.js';
import {OptionKind, SEARCH_OPTIONS_CHANGED, SearchOptionsChangedEvent, XfSearchOptionsElement} from '../widgets/xf_search_options.js';

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
  // The current value of search options, initialized to some sensible default.
  private currentOptions_: SearchOptions = getDefaultSearchOptions();
  // The store which updates us about state changes.
  private store_: Store;
  // The cached state of the store; store may post events if other parts of the
  // state change. However, we just want to react to changes related to search.
  // We use the cached search state to check if the state change was related to
  // search state change or some other part of the state.
  private searchState_: SearchData|undefined = undefined;
  // The container with search options widget.
  private optionsContainer_: HTMLElement;
  // The UI widget that allows users to manipulate search options. This is used
  // mostly to cache the access to the actual element, rather than accessing it
  // via querySelector.
  private searchOptions_: XfSearchOptionsElement|null = null;
  // The container used to display the path of the currently selected file.
  private pathContainer_: HTMLElement;
  // The element that shows the path of the currently selected file.
  private pathDisplay_: XfPathDisplayElement|null = null;
  // Volume manager, used by us to resolve paths of selected entries.
  private volumeManager_: VolumeManager;


  /**
   * Builds a search container that creates and manages UI elements. This
   * container receives a reference to `searchWrapper` that contains query
   * UI elements and `optionsContainer` that has search options UI element.
   * It uses `searchWrapper` to fetch them by IDs. Once the UI elements are
   * found, this container makes itself the listener of the events that are
   * posted to by the UI elements and converts them to business logic. This
   * includes notifying listeners when the the search query changes or the
   * auto-complete item is selected.
   */
  constructor(
      volumeManager: VolumeManager, searchWrapper: HTMLElement,
      optionsContainer: HTMLElement, pathContainer: HTMLElement) {
    super();
    this.volumeManager_ = volumeManager;
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

    this.optionsContainer_ = optionsContainer;
    this.pathContainer_ = pathContainer;
    this.store_ = getStore();
    this.store_.subscribe(this);

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
   * Clears the current search query. If the query was not already empty, it
   * closes the search box.
   */
  clear() {
    const value = this.inputElement_.value;
    if (value !== '') {
      this.inputElement_.value = '';
      if (!util.isSearchV2Enabled()) {
        // Force query change flow only if V2 search is not enabled. This
        // is due to the fact that in the legacy search we listen to input
        // events from the text field, which are not posted when the value
        // is directly assigned a value. In the V2 search we listen to store
        // change events that cause the code path that handles clearing of
        // search to be executed.
        this.onQueryChanged_();
      }
      requestAnimationFrame(() => {
        this.closeSearch();
      });
    }
  }

  /**
   * Sets the new query. This method does not post events, even if the query
   * changed as a result.
   */
  setQuery(query: string) {
    this.inputElement_.value = query;
    this.openSearch();
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
   * A method invoked every time the store state changes.
   */
  onStateChanged(state: State) {
    this.handleSearchState_(state.search);
    this.handleSelectionState_(state);
  }

  /**
   * Handles changes in the search state of the store state. If the search
   * is not active it hides the UI elements. Otherwise, updates them
   * accordingly.
   */
  private handleSearchState_(search: SearchData|undefined) {
    if (this.searchState_ === search) {
      // Bail out early if the search part of the state has not changed.
      return;
    }
    // Cache the last received search state for future comparisons.
    this.searchState_ = search;
    if (!search) {
      this.closeSearch();
      return;
    }
    const query = search.query;
    if (query !== undefined && query !== this.getQuery()) {
      this.setQuery(query);
    }
    if (util.isSearchV2Enabled()) {
      const status = search.status;
      if (status === PropStatus.STARTED && query) {
        this.showOptions_();
        this.showPathDisplay_();
      }
    }
  }

  /**
   * Handles changes in the current directory part of the state. It uses it
   * to set the path of the currently selected element.
   */
  private handleSelectionState_(state: State) {
    if (!this.pathDisplay_) {
      return;
    }
    this.pathDisplay_.path = this.getSelectedPath_(state);
  }

  /**
   * Helper function that converts information stored in State
   * a path of the selected file or directory.
   */
  private getSelectedPath_(state: State): string {
    const keys = state.currentDirectory?.selection?.keys;
    if (!keys) {
      return '';
    }
    const fileData = state.allEntries[keys[0]!];
    if (!fileData) {
      return '';
    }
    const entry = fileData.entry;
    if (!entry) {
      return '';
    }
    const parts: PathComponent[] =
        PathComponent.computeComponentsFromEntry(entry, this.volumeManager_);
    return parts.map(p => p.name).join('/');
  }

  /**
   * Hides the element that allows users to manipulate search options.
   */
  private hideOptions_() {
    const element = this.getSearchOptionsElement_();
    if (element) {
      element.hidden = true;
    }
  }

  /**
   * Shows or creates the element that allows the user to manipulate search
   * options.
   */
  private showOptions_() {
    let element = this.getSearchOptionsElement_();
    if (!element) {
      element = this.createSearchOptionsElement_();
    }
    element.hidden = false;
  }

  private hidePathDisplay_() {
    const element = this.getPathDisplayElement_();
    if (element) {
      element.hidden = true;
    }
  }

  private showPathDisplay_() {
    let element = this.getPathDisplayElement_();
    if (!element) {
      element = this.createPathDisplayElement_();
    }
    element.hidden = false;
  }

  /**
   * Returns the path display element by either retuning the cached instance,
   * or fetching it by its tag. May return null.
   */
  private getPathDisplayElement_(): XfPathDisplayElement|null {
    if (!this.pathDisplay_) {
      this.pathDisplay_ = document.querySelector('xf-path-display');
    }
    return this.pathDisplay_;
  }

  private createPathDisplayElement_(): XfPathDisplayElement {
    const element = document.createElement('xf-path-display');
    this.pathContainer_.appendChild(element);
    this.pathDisplay_ = element;
    return element;
  }

  /**
   * Returns the search options element by either retuning the cached instance,
   * or fetching it by its tag. May return null.
   */
  private getSearchOptionsElement_(): XfSearchOptionsElement|null {
    if (!this.searchOptions_) {
      this.searchOptions_ = document.querySelector('xf-search-options');
    }
    return this.searchOptions_;
  }

  private createSearchOptionsElement_(): XfSearchOptionsElement {
    const element = document.createElement('xf-search-options');
    this.optionsContainer_.appendChild(element);

    element.id = 'search-options';
    element.getLocationSelector().options = [
      {
        value: SearchLocation.EVERYWHERE,
        text: str('SEARCH_OPTIONS_LOCATION_EVERYWHERE'),
      },
      {
        value: SearchLocation.THIS_CHROMEBOOK,
        text: str('SEARCH_OPTIONS_LOCATION_THIS_CHROMEBOOK'),
      },
      {
        value: SearchLocation.THIS_FOLDER,
        text: str('SEARCH_OPTIONS_LOCATION_THIS_FOLDER'),
        default: true,
      },
    ];
    element.getRecencySelector().options = [
      {
        value: SearchRecency.ANYTIME,
        text: str('SEARCH_OPTIONS_RECENCY_ALL_TIME'),
      },
      {
        value: SearchRecency.TODAY,
        text: str('SEARCH_OPTIONS_RECENCY_TODAY'),
      },
      {
        value: SearchRecency.YESTERDAY,
        text: str('SEARCH_OPTIONS_RECENCY_YESTERDAY'),
      },
      {
        value: SearchRecency.LAST_WEEK,
        text: str('SEARCH_OPTIONS_RECENCY_LAST_WEEK'),
      },
      {
        value: SearchRecency.LAST_MONTH,
        text: str('SEARCH_OPTIONS_RECENCY_LAST_MONTH'),
      },
      {
        value: SearchRecency.LAST_YEAR,
        text: str('SEARCH_OPTIONS_RECENCY_LAST_YEAR'),
      },
    ];
    element.getFileTypeSelector().options = [
      {
        value: SearchFileType.ALL_TYPES,
        text: str('SEARCH_OPTIONS_TYPES_ALL_TYPES'),
      },
      {
        value: SearchFileType.AUDIO,
        text: str('SEARCH_OPTIONS_TYPES_AUDIO'),
      },
      {
        value: SearchFileType.DOCUMENTS,
        text: str('SEARCH_OPTIONS_TYPES_DOCUMENTS'),
      },
      {
        value: SearchFileType.IMAGES,
        text: str('SEARCH_OPTIONS_TYPES_IMAGES'),
      },
      {
        value: SearchFileType.VIDEOS,
        text: str('SEARCH_OPTIONS_TYPES_VIDEOS'),
      },
    ];
    element.addEventListener(
        SEARCH_OPTIONS_CHANGED, this.onOptionsChanged_.bind(this));
    this.searchOptions_ = element;
    return element;
  }

  private onOptionsChanged_(event: SearchOptionsChangedEvent) {
    const kind = event.detail.kind;
    const value = event.detail.value;
    switch (kind) {
      case OptionKind.LOCATION: {
        const location = value as unknown as SearchLocation;
        if (location !== this.currentOptions_.location) {
          this.currentOptions_.location = location;
          this.updateSearchOptions_();
        }
        break;
      }
      case OptionKind.RECENCY: {
        const recency = value as unknown as SearchRecency;
        if (recency !== this.currentOptions_.recency) {
          this.currentOptions_.recency = recency;
          this.updateSearchOptions_();
        }
        break;
      }
      case OptionKind.FILE_TYPE: {
        const type = value as unknown as SearchFileType;
        if (type !== this.currentOptions_.type) {
          this.currentOptions_.type = type;
          this.updateSearchOptions_();
        }
        break;
      }
      default:
        console.error(`Unhandled search option kind: ${kind}`);
        break;
    }
  }

  /**
   * Updates search options in the store.
   */
  private updateSearchOptions_() {
    if (util.isSearchV2Enabled()) {
      this.store_.dispatch(updateSearch({
        query: undefined,   // do not change
        status: undefined,  // do not change
        options: this.currentOptions_,
      }));
    }
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
      this.onQueryChanged_();
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
      this.searchButton_.focus();
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
  openSearch() {
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
      this.searchButton_.tabIndex = -1;
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
      this.hideOptions_();
      this.hidePathDisplay_();
      this.store_.dispatch(clearSearch());
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
      this.searchButton_.tabIndex = 0;
    }
  }

  /**
   * Generates a custom event with the current value of the input element as the
   * search query.
   */
  private onQueryChanged_() {
    const query = this.inputElement_.value.trimStart();
    this.dispatchEvent(new CustomEvent(SEARCH_QUERY_CHANGED, {
      bubbles: true,
      composed: true,
      detail: {
        query: query,
      },
    }));
    this.store_.dispatch(updateSearch({
      query: query,
      status: undefined,   // do not change
      options: undefined,  // do not change
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

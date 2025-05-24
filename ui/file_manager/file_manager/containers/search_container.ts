// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {VolumeManager} from '../background/js/volume_manager.js';
import {isModal} from '../common/js/dialog_type.js';
import {queryRequiredElement} from '../common/js/dom_utils.js';
import {isInsideDrive} from '../common/js/entry_utils.js';
import type {FakeEntry} from '../common/js/files_app_entry_types.js';
import {recordEnum, recordUserAction} from '../common/js/metrics.js';
import {str, strf} from '../common/js/translations.js';
import {RootType} from '../common/js/volume_manager_types.js';
import {PathComponent} from '../foreground/js/path_component.js';
import type {A11yAnnounce} from '../foreground/js/ui/a11y_announce.js';
import {changeDirectory} from '../state/ducks/current_directory.js';
import {clearSearch, getDefaultSearchOptions, isSearchEmpty, updateSearch} from '../state/ducks/search.js';
import type {FileKey} from '../state/file_key.js';
import {type CurrentDirectory, PropStatus, type SearchData, SearchLocation, type SearchOptions, SearchRecency, type State} from '../state/state.js';
import {getStore, type Store} from '../state/store.js';
import {type BreadcrumbClickedEvent, XfBreadcrumb} from '../widgets/xf_breadcrumb.js';
import type {XfSearchOptionsElement} from '../widgets/xf_search_options.js';
import {OptionKind, SEARCH_OPTIONS_CHANGED, type SearchOptionsChangedEvent} from '../widgets/xf_search_options.js';
import type {XfOption} from '../widgets/xf_select.js';

/**
 * Defines the possible states of the query input widget. This is a widget with
 * a search button, text input and clear button. By default, the widget is
 * closed. When active, it is open. Due to CSS transitions it has two
 * intermediate states, OPENING and CLOSING.
 */
enum SearchInputState {
  CLOSED = 'closed',
  OPEN = 'open',
}

enum SearchRootType {
  UNKNOWN = 'Unknown',
  STANDALONE = 'Standalone',
  PICKER = 'Picker',
}

/**
 * Helper function that centralizes the test if we are searching the "Recents"
 * directory.
 */
function isInRecent(dir: CurrentDirectory|undefined): boolean {
  return dir?.rootType === RootType.RECENT;
}

/**
 * Creates location options. These always consist of 'Everywhere' and the
 * local folder. However, if the local folder has a parent, that is different
 * from it, we also add the parent between Everywhere and the local folder.
 */
function createLocationOptions(state: State): XfOption[] {
  const dir = state.currentDirectory;
  const dirPath = dir?.pathComponents || [];
  const options: XfOption[] = [
    {
      value: SearchLocation.EVERYWHERE,
      text: str('SEARCH_OPTIONS_LOCATION_EVERYWHERE'),
      default: !dirPath,
    },
  ];
  if (dirPath.length > 0) {
    if (dir && isInsideDrive(dir)) {
      // For Google Drive we currently do not have the ability to search a
      // specific folder. Thus the only options shown, when the user is
      // triggering search from a location in Drive, is Everywhere (set up
      // above) and Drive.
      options.push({
        value: SearchLocation.ROOT_FOLDER,
        text: str('DRIVE_DIRECTORY_LABEL'),
        default: true,
      });
    } else if (isInRecent(dir)) {
      options.push({
        value: SearchLocation.THIS_FOLDER,
        text: dirPath[dirPath.length - 1]?.label ||
            str('SEARCH_OPTIONS_LOCATION_THIS_FOLDER'),
        default: true,
      });
    } else {
      options.push({
        value: dirPath.length > 1 ? SearchLocation.ROOT_FOLDER :
                                    SearchLocation.THIS_FOLDER,
        text: dirPath[0]?.label || str('SEARCH_OPTIONS_LOCATION_THIS_VOLUME'),
        default: dirPath.length === 1,
      });
      if (dirPath.length > 1) {
        options.push({
          value: SearchLocation.THIS_FOLDER,
          text: dirPath[dirPath.length - 1]?.label ||
              str('SEARCH_OPTIONS_LOCATION_THIS_FOLDER'),
          default: true,
        });
      }
    }
  }
  return options;
}

/**
 * Creates Recency options. Depending on the current directory these have either
 * ANYTIME or LAST_MONTH selected as the default.
 */
function createRecencyOptions(state: State): XfOption[] {
  const recencyOptions: XfOption[] = [
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
  const index = isInRecent(state.currentDirectory) ? 4 : 0;
  recencyOptions[index]!.default = true;
  return recencyOptions;
}

/**
 * Creates file category option. Uses the state to find out if we are in the
 * Recent view. If so, it sets the default based on the Recent view selectors.
 */
function createFileCategoryOptions(state: State): XfOption[] {
  let fileCategory = chrome.fileManagerPrivate.FileCategory.ALL;
  if (isInRecent(state.currentDirectory)) {
    const entry =
        state.allEntries[state.currentDirectory!.key]!.entry as FakeEntry;
    fileCategory = entry.fileCategory!;
  }
  return [
    {
      value: chrome.fileManagerPrivate.FileCategory.ALL,
      text: str('SEARCH_OPTIONS_TYPES_ALL_TYPES'),
      default: fileCategory === chrome.fileManagerPrivate.FileCategory.ALL,
    },
    {
      value: chrome.fileManagerPrivate.FileCategory.AUDIO,
      text: str('SEARCH_OPTIONS_TYPES_AUDIO'),
      default: fileCategory === chrome.fileManagerPrivate.FileCategory.AUDIO,
    },
    {
      value: chrome.fileManagerPrivate.FileCategory.DOCUMENT,
      text: str('SEARCH_OPTIONS_TYPES_DOCUMENTS'),
      default: fileCategory === chrome.fileManagerPrivate.FileCategory.DOCUMENT,
    },
    {
      value: chrome.fileManagerPrivate.FileCategory.IMAGE,
      text: str('SEARCH_OPTIONS_TYPES_IMAGES'),
      default: fileCategory === chrome.fileManagerPrivate.FileCategory.IMAGE,
    },
    {
      value: chrome.fileManagerPrivate.FileCategory.VIDEO,
      text: str('SEARCH_OPTIONS_TYPES_VIDEOS'),
      default: fileCategory === chrome.fileManagerPrivate.FileCategory.VIDEO,
    },
  ];
}

/**
 * Updates visibility of recency options based on the current directory.
 */
function updateRecencyOptionsVisibility(
    state: State, element: XfSearchOptionsElement, options: SearchOptions) {
  if (isInRecent(state.currentDirectory)) {
    const recencySelector = element.getRecencySelector();
    if (options.location === SearchLocation.EVERYWHERE) {
      recencySelector.toggleAttribute('hidden', false);
    } else {
      recencySelector.toggleAttribute('hidden', true);
    }
  }
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
  private breadcrumb_: XfBreadcrumb|null = null;
  // The parts of the path of the selected result or empty.
  private pathComponents_: FileKey[] = [];
  // Volume manager, used by us to resolve paths of selected entries.
  private volumeManager_: VolumeManager;
  // The accessibility interface that is used to announce the outcomes of file
  // searches.
  private a11y_: A11yAnnounce;


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
      optionsContainer: HTMLElement, pathContainer: HTMLElement,
      a11y: A11yAnnounce) {
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
        this.searchBox_.querySelector<CrInputElement>('cr-input')!;

    // The button that allows the user to clear the query.
    this.clearButton_ = this.searchBox_.querySelector<HTMLElement>('.clear')!;
    // Hide clear button when created.
    this.updateClearButton_('');

    this.optionsContainer_ = optionsContainer;
    this.pathContainer_ = pathContainer;
    this.a11y_ = a11y;
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
      requestAnimationFrame(() => {
        this.closeSearch();
      });
    }
  }

  /**
   * Returns the user entered search query. This method trims white spaces from
   * the left side of the query.
   */
  getQuery(): string {
    return this.inputElement_.value.trimStart();
  }

  /**
   * A method invoked every time the store state changes.
   */
  onStateChanged(state: State) {
    this.handleSearchState_(state);
    this.handleSelectionState_(state);
  }

  /**
   * Handles changes in the search state of the store state. If the search
   * is not active it hides the UI elements. Otherwise, updates them
   * accordingly.
   */
  private handleSearchState_(state: State) {
    const search = state.search;
    if (this.searchState_ === search) {
      // Bail out early if the search part of the state has not changed.
      return;
    }
    // Cache the last received search state for future comparisons.
    const lastSearch = this.searchState_;
    this.searchState_ = state.search;

    if (lastSearch?.query && search && search.query === undefined) {
      this.a11y_.speakA11yMessage(str('SEARCH_A11Y_CLEAR_SEARCH'));
    }
    if (!search || isSearchEmpty(search)) {
      this.closeSearch();
      return;
    }

    const query = search.query;
    if (query !== undefined && query !== this.getQuery()) {
      this.inputElement_.value = query;
      this.openSearch();
    }
    if (search.status === PropStatus.STARTED && query) {
      this.showOptionsElement_(state);
      this.showBreadcrumbElement_();
    }
    if (search.status === PropStatus.SUCCESS && query) {
      const content = state.currentDirectory?.content;
      const count = content ? content.keys.length : 0;
      const messageId =
          count === 0 ? 'SEARCH_A11Y_NO_RESULT' : 'SEARCH_A11Y_RESULT';
      this.a11y_.speakA11yMessage(strf(messageId, query));
    }
  }

  /**
   * Handles changes in the current directory part of the state. It uses it
   * to set the path of the currently selected element.
   */
  private handleSelectionState_(state: State) {
    const search = state.search;
    if (!search || !search.query) {
      return;
    }
    if (!this.breadcrumb_) {
      this.showBreadcrumbElement_();
    }
    const parts = this.getPathComponentsOfSelectedEntry_(state);
    const path = parts.map(p => p.name).join('/');
    this.pathComponents_ = parts.map(p => p.getKey());
    if (path) {
      this.breadcrumb_!.removeAttribute('hidden');
      this.breadcrumb_!.path = path;
    } else {
      this.breadcrumb_!.path = '';
      this.breadcrumb_!.setAttribute('hidden', '');
    }
  }

  /**
   * Helper function that converts information stored in State to an array
   * of PathComponents of the selected entry. If there are multiple entries
   * selected or no entries selected, this method returns an empty array.
   */
  private getPathComponentsOfSelectedEntry_(state: State): PathComponent[] {
    const keys = state.currentDirectory?.selection?.keys;
    if (!keys || keys.length !== 1) {
      return [];
    }
    const entry = state.allEntries[keys[0]!]?.entry;
    if (!entry) {
      return [];
    }
    // TODO(b:274559834): Improve efficiency of these computations.
    return PathComponent.computeComponentsFromEntry(entry, this.volumeManager_);
  }

  /**
   * Hides the element that allows users to manipulate search options.
   */
  private hideOptionsElement_() {
    if (this.searchOptions_) {
      this.searchOptions_.remove();
      this.searchOptions_ = null;
    }
  }

  /**
   * Shows or creates the element that allows the user to manipulate search
   * options.
   */
  private showOptionsElement_(state: State) {
    let element = this.getSearchOptionsElement_();
    if (!element) {
      element = this.createSearchOptionsElement_(state);
    }
    element.hidden = false;
  }

  private hideBreadcrumbElement_() {
    const element = this.getBreadcrumbElement_();
    if (element) {
      element.hidden = true;
    }
  }

  private showBreadcrumbElement_() {
    let element = this.getBreadcrumbElement_();
    if (!element) {
      element = this.createBreadcrumbElement_();
    }
    element.hidden = false;
  }

  /**
   * Returns the breadcrumb element by either retuning the cached instance,
   * or fetching it by its tag. May return null.
   */
  private getBreadcrumbElement_(): XfBreadcrumb|null {
    if (!this.breadcrumb_) {
      this.breadcrumb_ = document.querySelector('xf-breadcumb');
    }
    return this.breadcrumb_;
  }

  private createBreadcrumbElement_(): XfBreadcrumb {
    const element = new XfBreadcrumb();
    // Increase the default maxPathParts to allow for longer path display.
    element.maxPathParts = 100;
    element.id = 'search-breadcrumb';
    element.small = true;
    element.addEventListener(
        XfBreadcrumb.events.BREADCRUMB_CLICKED,
        this.breadcrumbClick_.bind(this));
    this.pathContainer_.appendChild(element);
    this.breadcrumb_ = element;
    return element;
  }

  private breadcrumbClick_(event: BreadcrumbClickedEvent) {
    const index = Number(event.detail.partIndex);
    if (isNaN(index) || index < 0) {
      return;
    }
    // The leaf path isn't clickable.
    if (index >= this.pathComponents_.length - 1) {
      return;
    }

    this.store_.dispatch(
        changeDirectory({toKey: this.pathComponents_[index] as FileKey}));
    recordUserAction('ClickBreadcrumbs');
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

  private createSearchOptionsElement_(state: State): XfSearchOptionsElement {
    const element = document.createElement('xf-search-options');
    this.optionsContainer_.appendChild(element);
    element.id = 'search-options';
    element.getLocationSelector().options = createLocationOptions(state);
    element.getRecencySelector().options = createRecencyOptions(state);
    element.getFileTypeSelector().options = createFileCategoryOptions(state);
    this.updateSearchOptions_(state);
    element.addEventListener(
        SEARCH_OPTIONS_CHANGED, this.onOptionsChanged_.bind(this));
    this.searchOptions_ = element;
    return element;
  }

  private onOptionsChanged_(event: SearchOptionsChangedEvent) {
    const kind = event.detail.kind;
    const value = event.detail.value;
    const state = this.store_.getState();
    switch (kind) {
      case OptionKind.LOCATION: {
        const location = value as unknown as SearchLocation;
        if (location !== this.currentOptions_.location) {
          this.currentOptions_.location = location;
          this.updateSearchOptions_(state);
        }
        break;
      }
      case OptionKind.RECENCY: {
        const recency = value as unknown as SearchRecency;
        if (recency !== this.currentOptions_.recency) {
          this.currentOptions_.recency = recency;
          this.updateSearchOptions_(state);
        }
        break;
      }
      case OptionKind.FILE_TYPE: {
        const category =
            value as unknown as chrome.fileManagerPrivate.FileCategory;
        if (category !== this.currentOptions_.fileCategory) {
          this.currentOptions_.fileCategory = category;
          this.updateSearchOptions_(state);
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
  private updateSearchOptions_(state: State) {
    updateRecencyOptionsVisibility(
        state, this.getSearchOptionsElement_()!, this.currentOptions_);
    this.store_.dispatch(updateSearch({
      query: this.getQuery(),
      status: undefined,  // do not change
      options: this.currentOptions_,
    }));
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
      }
    });
    this.inputElement_.addEventListener('input', () => {
      this.onQueryChanged_();
    });
    this.inputElement_.addEventListener('keydown', (event: KeyboardEvent) => {
      if (!this.inputElement_.value) {
        if (event.key === 'Escape') {
          this.closeSearch();
          this.searchButton_.focus();
        }
        if (event.key === 'Tab') {
          this.closeSearch();
        }
      }
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
  }

  /**
   * Returns whether the search container is open. In the open state the user
   * may enter a search query, interact with options, etc.
   */
  isOpen() {
    return this.inputState_ === SearchInputState.OPEN;
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
      this.inputState_ = SearchInputState.OPEN;
      this.inputElement_.addEventListener('transitionend', () => {
        this.searchWrapper_.removeAttribute('collapsed');
      }, {once: true, passive: true, capture: true});
      this.inputElement_.disabled = false;
      this.inputElement_.tabIndex = 0;
      this.inputElement_.focus();
      this.searchWrapper_.classList.add('has-cursor', 'has-text');
      this.searchBox_.classList.add('has-cursor', 'has-text');
      this.searchButton_.tabIndex = -1;
      this.updateClearButton_(this.getQuery());
      const dialogType = this.store_.getState().launchParams.dialogType;
      const rootType: SearchRootType = dialogType ?
          (isModal(dialogType) ? SearchRootType.PICKER :
                                 SearchRootType.STANDALONE) :
          SearchRootType.UNKNOWN;
      recordEnum('Search.RootType', rootType, [
        SearchRootType.UNKNOWN,
        SearchRootType.STANDALONE,
        SearchRootType.PICKER,
      ]);
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
      this.inputState_ = SearchInputState.CLOSED;
      this.inputElement_.addEventListener('transitionend', () => {
        this.searchWrapper_.setAttribute('collapsed', '');
      }, {once: true, passive: true, capture: true});
      this.hideOptionsElement_();
      this.hideBreadcrumbElement_();
      this.store_.dispatch(clearSearch());
      this.inputElement_.tabIndex = -1;
      this.inputElement_.disabled = true;
      this.inputElement_.blur();
      this.inputElement_.value = '';
      this.searchWrapper_.classList.remove('has-cursor', 'has-text');
      this.searchBox_.classList.remove('has-cursor', 'has-text');
      this.searchButton_.tabIndex = 0;
      this.currentOptions_ = getDefaultSearchOptions();
    }
  }

  /**
   * Updates the visibility of clear button.
   */
  private updateClearButton_(query: string) {
    this.clearButton_.hidden = (query.length <= 0);
  }

  /**
   * Generates a custom event with the current value of the input element as the
   * search query.
   */
  private onQueryChanged_() {
    const query = this.inputElement_.value.trimStart();
    this.updateClearButton_(query);
    this.store_.dispatch(updateSearch({
      query: query,
      status: undefined,  // do not change
      options: this.currentOptions_,
    }));
  }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_match.js';
import './searchbox_dropdown_shared_style.css.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import {getTemplate} from './searchbox_dropdown.html.js';
import type {SearchboxMatchElement} from './searchbox_match.js';
import type {AutocompleteMatch, AutocompleteResult, OmniboxPopupSelection, PageHandlerInterface} from './searchbox.mojom-webui.js';
import {RenderType, SelectionLineState, SideType} from './searchbox.mojom-webui.js';
import {decodeString16, renderTypeToClass, sideTypeToClass} from './utils.js';

// The '%' operator in JS returns negative numbers. This workaround avoids that.
const remainder = (lhs: number, rhs: number) => ((lhs % rhs) + rhs) % rhs;

const CHAR_TYPED_TO_PAINT = 'Realbox.CharTypedToRepaintLatency.ToPaint';
const RESULT_CHANGED_TO_PAINT = 'Realbox.ResultChangedToRepaintLatency.ToPaint';

export interface SearchboxDropdownElement {
  $: {
    content: HTMLElement,
  };
}

// A dropdown element that contains autocomplete matches. Provides an API for
// the embedder (i.e., <cr-searchbox>) to change the selection.
export class SearchboxDropdownElement extends PolymerElement {
  static get is() {
    return 'cr-searchbox-dropdown';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        computed: `computeHasSecondarySide_(result)`,
        notify: true,
        reflectToAttribute: true,
      },

      hasEmptyInput: {
        type: Boolean,
        reflectToAttribute: true,
        computed: `computeHasEmptyInput_(result)`,
      },

      result: {
        type: Object,
      },

      /** Index of the selected match. */
      selectedMatchIndex: {
        type: Number,
        value: -1,
        notify: true,
      },

      /**
       * Computed value for whether or not the dropdown should show the
       * secondary side. This depends on whether the parent has set
       * `canShowSecondarySide` to true and whether there are visible primary
       * matches.
       */
      showSecondarySide_: {
        type: Boolean,
        value: false,
        computed: 'computeShowSecondarySide_(' +
            'canShowSecondarySide, result.matches.*, hiddenGroupIds_.*)',
      },

      showThumbnail: {
        type: Boolean,
        value: false,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /** The list of suggestion group IDs whose matches should be hidden. */
      hiddenGroupIds_: {
        type: Array,
        computed: `computeHiddenGroupIds_(result)`,
      },

      /** The list of selectable match elements. */
      selectableMatchElements_: {
        type: Array,
        value: () => [],
      },
    };
  }

  canShowSecondarySide: boolean;
  hadSecondarySide: boolean;
  hasSecondarySide: boolean;
  result: AutocompleteResult;
  selectedMatchIndex: number;
  private hiddenGroupIds_: number[];
  private selectableMatchElements_: SearchboxMatchElement[];
  private showSecondarySide_: boolean;
  private resizeObserver_: ResizeObserver|null = null;
  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver_ = new ResizeObserver(
        (entries: ResizeObserverEntry[]) =>
            this.pageHandler_.popupElementSizeChanged({
              width: entries[0].contentRect.width,
              height: entries[0].contentRect.height,
            }));
    this.resizeObserver_.observe(this.$.content);
  }

  override disconnectedCallback() {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    super.disconnectedCallback();
  }

  //============================================================================
  // Public methods
  //============================================================================

  /** Filters out secondary matches, if any, unless they can be shown. */
  get selectableMatchElements() {
    return this.selectableMatchElements_.filter(
        matchEl => matchEl.sideType === SideType.kDefaultPrimary ||
            this.showSecondarySide_);
  }

  /** Unselects the currently selected match, if any. */
  unselect() {
    this.selectedMatchIndex = -1;
  }

  /** Focuses the selected match, if any. */
  focusSelected() {
    this.selectableMatchElements[this.selectedMatchIndex]?.focus();
  }

  /** Selects the first match. */
  selectFirst() {
    this.selectedMatchIndex = 0;
  }

  /** Selects the match at the given index. */
  selectIndex(index: number) {
    this.selectedMatchIndex = index;
  }

  updateSelection(
      oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    if (selection.state === SelectionLineState.kFocusedButtonHeader) {
      // TODO: Focus group header.
      this.unselect();
      return;
    }
    // If the updated selection is a new match, remove any remaining selection
    // on the previously selected match.
    if (oldSelection.line !== selection.line) {
      this.selectableMatchElements[this.selectedMatchIndex]?.updateSelection(
          selection);
    }
    this.selectIndex(selection.line);
    this.selectableMatchElements[this.selectedMatchIndex]?.updateSelection(
        selection);
  }

  /**
   * Selects the previous match with respect to the currently selected one.
   * Selects the last match if the first one or no match is currently selected.
   */
  selectPrevious() {
    // The value of -1 for |this.selectedMatchIndex| indicates no selection.
    // Therefore subtract one from the maximum of its value and 0.
    const previous = Math.max(this.selectedMatchIndex, 0) - 1;
    this.selectedMatchIndex =
        remainder(previous, this.selectableMatchElements.length);
  }

  /** Selects the last match. */
  selectLast() {
    this.selectedMatchIndex = this.selectableMatchElements.length - 1;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one or no match is currently selected.
   */
  selectNext() {
    const next = this.selectedMatchIndex + 1;
    this.selectedMatchIndex =
        remainder(next, this.selectableMatchElements.length);
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onHeaderClick_(e: Event) {
    const groupId =
        Number.parseInt((e.currentTarget as HTMLElement).dataset['id']!, 10);

    // Tell the backend to toggle visibility of the given suggestion group ID.
    this.pageHandler_.toggleSuggestionGroupIdVisibility(groupId);

    // Hide/Show matches with the given suggestion group ID.
    const index = this.hiddenGroupIds_.indexOf(groupId);
    if (index === -1) {
      this.push('hiddenGroupIds_', groupId);
    } else {
      this.splice('hiddenGroupIds_', index, 1);
    }
  }

  private onHeaderFocusin_() {
    this.dispatchEvent(new CustomEvent('header-focusin', {
      bubbles: true,
      composed: true,
    }));
  }

  private onHeaderMousedown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  private onResultRepaint_() {
    if (loadTimeData.getBoolean('reportMetrics')) {
      const metricsReporter = MetricsReporterImpl.getInstance();
      metricsReporter.measure('CharTyped')
          .then(duration => {
            metricsReporter.umaReportTime(CHAR_TYPED_TO_PAINT, duration);
          })
          .then(() => {
            metricsReporter.clearMark('CharTyped');
          })
          .catch(() => {});  // Fail silently if 'CharTyped' is not marked.

      metricsReporter.measure('ResultChanged')
          .then(duration => {
            metricsReporter.umaReportTime(RESULT_CHANGED_TO_PAINT, duration);
          })
          .then(() => {
            metricsReporter.clearMark('ResultChanged');
          })
          .catch(() => {});  // Fail silently if 'ResultChanged' is not marked.
    }

    // Update the list of selectable match elements.
    this.selectableMatchElements_ =
        [...this.shadowRoot!.querySelectorAll('cr-searchbox-match')];
  }

  //============================================================================
  // Helpers
  //============================================================================

  private sideTypeClass_(side: SideType): string {
    return sideTypeToClass(side);
  }

  private renderTypeClassForGroup_(groupId: number): string {
    return renderTypeToClass(
        this.result?.suggestionGroupsMap[groupId]?.renderType ??
        RenderType.kDefaultVertical);
  }

  private computeHasSecondarySide_(): boolean {
    const hasSecondarySide =
        !!this.groupIdsForSideType_(SideType.kSecondary).length;
    if (!this.hadSecondarySide) {
      this.hadSecondarySide = hasSecondarySide;
    }
    return hasSecondarySide;
  }

  private computeHasEmptyInput_(): boolean {
    return this.result && decodeString16(this.result.input) === '';
  }

  private computeHiddenGroupIds_(): number[] {
    return Object.keys(this.result?.suggestionGroupsMap ?? {})
        .map(groupId => Number.parseInt(groupId, 10))
        .filter(groupId => this.result.suggestionGroupsMap[groupId].hidden);
  }

  private isSelected_(match: AutocompleteMatch): boolean {
    return this.matchIndex_(match) === this.selectedMatchIndex;
  }

  /**
   * @returns The unique suggestion group IDs that belong to the given side type
   *     while preserving the order in which they appear in the list of matches.
   */
  private groupIdsForSideType_(side: SideType): number[] {
    return [...new Set<number>(
        this.result?.matches?.map(match => match.suggestionGroupId)
            .filter(groupId => this.sideTypeForGroup_(groupId) === side))];
  }

  /**
   * @returns Whether matches with the given suggestion group ID should be
   *     hidden.
   */
  private groupIsHidden_(groupId: number): boolean {
    return this.hiddenGroupIds_.indexOf(groupId) !== -1;
  }

  /**
   * @returns Whether the given suggestion group ID has a header.
   */
  private hasHeaderForGroup_(groupId: number): boolean {
    return !!this.headerForGroup_(groupId);
  }

  /**
   * @returns The header for the given suggestion group ID, if any.
   */
  private headerForGroup_(groupId: number): string {
    return this.result?.suggestionGroupsMap[groupId] ?
        decodeString16(this.result.suggestionGroupsMap[groupId].header) :
        '';
  }

  /**
   * @returns Index of the match in the autocomplete result. Passed to the match
   *     so it knows its position in the list of matches.
   */
  private matchIndex_(match: AutocompleteMatch): number {
    return this.result?.matches?.indexOf(match) ?? -1;
  }

  /**
   * @returns The list of visible matches that belong to the given suggestion
   *     group ID.
   */
  private matchesForGroup_(groupId: number): AutocompleteMatch[] {
    return this.groupIsHidden_(groupId) ?
        [] :
        (this.result?.matches ??
         []).filter(match => match.suggestionGroupId === groupId);
  }

  /**
   * @returns The list of side types to show.
   */
  private sideTypes_(): SideType[] {
    return this.showSecondarySide_ ?
        [SideType.kDefaultPrimary, SideType.kSecondary] :
        [SideType.kDefaultPrimary];
  }

  /**
   * @returns The side type for the given suggestion group ID.
   */
  private sideTypeForGroup_(groupId: number): SideType {
    return this.result?.suggestionGroupsMap[groupId]?.sideType ??
        SideType.kDefaultPrimary;
  }

  /**
   * @returns A11y label for suggestion group show/hide toggle button.
   */
  private toggleButtonA11yLabelForGroup_(groupId: number): string {
    if (!this.hasHeaderForGroup_(groupId)) {
      return '';
    }

    return !this.groupIsHidden_(groupId) ?
        decodeString16(
            this.result.suggestionGroupsMap[groupId].hideGroupA11yLabel) :
        decodeString16(
            this.result.suggestionGroupsMap[groupId].showGroupA11yLabel);
  }

  /**
   * @returns Icon name for suggestion group show/hide toggle button.
   */
  private toggleButtonIconForGroup_(groupId: number): string {
    return this.groupIsHidden_(groupId) ? 'icon-arrow-drop-down-cr23' :
                                          'icon-arrow-drop-up-cr23';
  }

  /**
   * @returns Tooltip for suggestion group show/hide toggle button.
   */
  private toggleButtonTitleForGroup_(groupId: number): string {
    return loadTimeData.getString(
        this.groupIsHidden_(groupId) ? 'showSuggestions' : 'hideSuggestions');
  }

  private computeShowSecondarySide_(): boolean {
    if (!this.canShowSecondarySide) {
      // Parent prohibits showing secondary side.
      return false;
    }

    if (!this.hiddenGroupIds_) {
      // Not ready yet as dropdown has received results but has not yet
      // determined which groups are hidden.
      return true;
    }

    // Only show secondary side if there are primary matches visible.
    const primaryGroupIds = this.groupIdsForSideType_(SideType.kDefaultPrimary);
    return primaryGroupIds.some((groupId) => {
      return this.matchesForGroup_(groupId).length > 0;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-dropdown': SearchboxDropdownElement;
  }
}

customElements.define(SearchboxDropdownElement.is, SearchboxDropdownElement);

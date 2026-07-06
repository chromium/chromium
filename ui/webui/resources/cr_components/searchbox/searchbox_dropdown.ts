// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_match.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, OmniboxPopupSelection} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {RenderType, SelectionDirection, SelectionLineState, SelectionStep, SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox_dropdown.css.js';
import {getHtml} from './searchbox_dropdown.html.js';
import {kDefaultSelection} from './searchbox_match.js';
import type {SearchboxMatchElement} from './searchbox_match.js';
import {renderTypeToClass, sideTypeToClass} from './utils.js';

// The '%' operator in JS returns negative numbers. This workaround avoids that.
const remainder = (lhs: number, rhs: number) => ((lhs % rhs) + rhs) % rhs;

export interface SearchboxDropdownElement {
  $: {
    content: HTMLElement,
  };
}

// A dropdown element that contains autocomplete matches. Provides an API for
// the embedder (i.e., <cr-searchbox>) to change the selection.
export class SearchboxDropdownElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-dropdown';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {type: Boolean},

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      hasEmptyInput: {
        type: Boolean,
        reflect: true,
      },

      result: {type: Object},

      // TODO(crbug.com/519713849): Remove selectedMatchIndex once
      // kRealboxVirtualFocusNavigation is launched.
      selectedMatchIndex: {
        type: Number,
        notify: true,
      },

      /** Omnibox focused selection state. */
      selection: {
        type: Object,
        notify: true,
      },

      showThumbnail: {type: Boolean},

      virtualFocusEnabled: {type: Boolean},

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Computed value for whether or not the dropdown should show the
       * secondary side. This depends on whether the parent has set
       * `canShowSecondarySide` to true and whether there are visible primary
       * matches.
       */
      showSecondarySide_: {type: Boolean},
    };
  }

  accessor canShowSecondarySide: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor hasEmptyInput: boolean = false;
  accessor result: AutocompleteResult|null = null;
  // TODO(crbug.com/519713849): Remove selectedMatchIndex once
  // kRealboxVirtualFocusNavigation is launched.
  accessor selectedMatchIndex: number = -1;
  accessor selection: OmniboxPopupSelection = kDefaultSelection;
  accessor showThumbnail: boolean = false;
  accessor virtualFocusEnabled: boolean =
      loadTimeData.valueExists('realboxVirtualFocusNavigation') &&
      loadTimeData.getBoolean('realboxVirtualFocusNavigation');
  private accessor showSecondarySide_: boolean = false;

  /** The list of selectable match elements. */
  private selectableMatchElements_: SearchboxMatchElement[] = [];

  private availableSelections_: OmniboxPopupSelection[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('result')) {
      this.hasSecondarySide = this.computeHasSecondarySide_();
      this.hasEmptyInput = this.computeHasEmptyInput_();
    }

    if (changedProperties.has('result') ||
        changedProperties.has('canShowSecondarySide')) {
      this.showSecondarySide_ = this.computeShowSecondarySide_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('result')) {
      this.availableSelections_ = this.getResultSelections();
    }

    this.onResultRepaint_();

    // Update the list of selectable match elements.
    this.selectableMatchElements_ =
        [...this.shadowRoot.querySelectorAll('cr-searchbox-match')];
  }

  getCachedSelections(): OmniboxPopupSelection[] {
    return this.availableSelections_;
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
    this.selection = kDefaultSelection;
  }

  /** Focuses the selected match, if any. */
  focusSelected() {
    this.selectableMatchElements[this.selectedMatchIndex]?.focus();
  }

  /** Selects the first match. */
  selectFirst() {
    return this.selectIndex(0);
  }

  /** Selects the match at the given index. */
  selectIndex(index: number) {
    this.selectedMatchIndex = index;
    if (this.virtualFocusEnabled && this.selection.line !== index) {
      const match = this.result?.matches[index];
      if (match && (!match.isHidden || match.allowedToBeDefaultMatch)) {
        this.selection = {
          line: index,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        };
      } else {
        this.selection = kDefaultSelection;
      }
    }
    return this.updateComplete;
  }

  updateSelection(
      _oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    this.selectIndex(selection.line);
    this.fire('selection-changed', {value: selection});
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
    return this.updateComplete;
  }

  /** Selects the last match. */
  selectLast() {
    this.selectedMatchIndex = this.selectableMatchElements.length - 1;
    return this.updateComplete;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one or no match is currently selected.
   */
  selectNext() {
    const next = this.selectedMatchIndex + 1;
    this.selectedMatchIndex =
        remainder(next, this.selectableMatchElements.length);
    return this.updateComplete;
  }

  stepSelection(direction: SelectionDirection, step: SelectionStep) {
    if (!this.result) {
      return;
    }
    const available = this.getCachedSelections();
    if (available.length === 0) {
      return;
    }
    const current = this.selection;
    const isNormal = (selection: OmniboxPopupSelection) =>
        selection.state === SelectionLineState.kNormal;
    const selectionsEqual =
        (a: OmniboxPopupSelection, b: OmniboxPopupSelection) =>
            a.line === b.line && a.state === b.state &&
        a.actionIndex === b.actionIndex;

    let fromIndex = available.findIndex(s => selectionsEqual(current, s));

    // Fallback: If currently in keyword mode, the keyword chip might be hidden
    // from the match list (as we have transitioned inside that engine search).
    // Find the normal suggestion line as the starting point for navigation.
    if (fromIndex < 0 && current.state === SelectionLineState.kKeywordMode) {
      fromIndex = available.findIndex(
          s => selectionsEqual(
              {
                ...current,
                state: SelectionLineState.kNormal,
              },
              s));
    }
    let nextSelection = current;
    if (fromIndex < 0) {
      // Create a copy of the available selections array before modifying it
      // to avoid modifying the cached array.
      const availableCopy = [...available];
      availableCopy.splice(0, 0, current);
      fromIndex = 0;
      nextSelectionFromAvailable(availableCopy);
    } else {
      nextSelectionFromAvailable(available);
    }

    function nextSelectionFromAvailable(
        selectionsList: OmniboxPopupSelection[]) {
      if (step === SelectionStep.kAllLines) {
        const normalIndex = direction === SelectionDirection.kBackward ?
            selectionsList.findIndex(isNormal) :
            selectionsList.findLastIndex(isNormal);
        nextSelection =
            normalIndex < 0 ? current : selectionsList[normalIndex]!;
      } else {
        const getNextIndex = (idx: number): number => {
          const stepSize = direction === SelectionDirection.kForward ? 1 : -1;
          return remainder(idx + stepSize, selectionsList.length);
        };

        const originalIndex = fromIndex;
        let index = getNextIndex(originalIndex);
        do {
          const candidate = selectionsList[index]!;
          if (step === SelectionStep.kStateOrLine || isNormal(candidate)) {
            nextSelection = candidate;
            break;
          }
          index = getNextIndex(index);
        } while (index !== originalIndex);
      }
    }

    const oldSelection = this.selection;
    this.selection = nextSelection;
    this.updateSelection(oldSelection, nextSelection);
  }

  /**
   * Returns a flat array of all selectable target states in the dropdown
   * (including normal lines, keyword mode chips, action chips, and remove
   * buttons) in order.
   */
  private getSelectionsForMatch_(match: AutocompleteMatch, matchIndex: number):
      OmniboxPopupSelection[] {
    if (match.isHidden && !match.allowedToBeDefaultMatch) {
      return [];
    }
    const selections: OmniboxPopupSelection[] = [{
      line: matchIndex,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    }];
    if (match.keywordChipHint && match.keywordChipHint.length > 0) {
      selections.push({
        line: matchIndex,
        state: SelectionLineState.kKeywordMode,
        actionIndex: 0,
      });
    }
    if (match.actions) {
      for (let actionIndex = 0; actionIndex < match.actions.length;
           actionIndex++) {
        selections.push({
          line: matchIndex,
          state: SelectionLineState.kFocusedButtonAction,
          actionIndex: actionIndex,
        });
      }
    }
    if (match.supportsDeletion) {
      selections.push({
        line: matchIndex,
        state: SelectionLineState.kFocusedButtonRemoveSuggestion,
        actionIndex: 0,
      });
    }
    return selections;
  }

  getResultSelections(): OmniboxPopupSelection[] {
    if (!this.result) {
      return [];
    }
    return this.result.matches.flatMap(
        (match: AutocompleteMatch, matchIndex: number) =>
            this.getSelectionsForMatch_(match, matchIndex));
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onHeaderMousedown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  private onResultRepaint_() {
    if (!loadTimeData.getBoolean('reportMetrics')) {
      return;
    }

    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('CharTyped')
        .then(duration => {
          metricsReporter.umaReportTime(
              loadTimeData.getString('charTypedToPaintMetricName'), duration);
        })
        .then(() => {
          metricsReporter.clearMark('CharTyped');
        })
        .catch(() => {});  // Fail silently if 'CharTyped' is not marked.

    metricsReporter.measure('ResultChanged')
        .then(duration => {
          metricsReporter.umaReportTime(
              loadTimeData.getString('resultChangedToPaintMetricName'),
              duration);
        })
        .then(() => {
          metricsReporter.clearMark('ResultChanged');
        })
        .catch(() => {});  // Fail silently if 'ResultChanged' is not marked.
  }

  //============================================================================
  // Helpers
  //============================================================================

  protected sideTypeClass_(side: SideType): string {
    return sideTypeToClass(side);
  }

  protected renderTypeClassForGroup_(groupId: number): string {
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
    return !!this.result && this.result.input === '';
  }

  protected isSelected_(match: AutocompleteMatch): boolean {
    return this.matchIndex_(match) === this.selectedMatchIndex;
  }

  /**
   * @returns The unique suggestion group IDs that belong to the given side type
   *     while preserving the order in which they appear in the list of matches.
   */
  protected groupIdsForSideType_(side: SideType): number[] {
    return [...new Set<number>(
        this.result?.matches.map(match => match.suggestionGroupId)
            .filter(groupId => this.sideTypeForGroup_(groupId) === side))];
  }

  /**
   * @returns Whether the given suggestion group ID has a header.
   */
  protected hasHeaderForGroup_(groupId: number): boolean {
    return !!this.headerForGroup_(groupId);
  }

  protected getAriaDescribedByForGroup_(groupId: number): string {
    return this.hasHeaderForGroup_(groupId) ? `hg_${groupId}` : '';
  }

  /**
   * @returns The header for the given suggestion group ID, if any.
   */
  protected headerForGroup_(groupId: number): string {
    return this.result?.suggestionGroupsMap[groupId] ?
        this.result.suggestionGroupsMap[groupId].header :
        '';
  }

  /**
   * @returns Index of the match in the autocomplete result. Passed to the match
   *     so it knows its position in the list of matches.
   */
  protected matchIndex_(match: AutocompleteMatch): number {
    return this.result?.matches.indexOf(match) ?? -1;
  }

  /**
   * @returns The list of visible matches that belong to the given suggestion
   *     group ID.
   */
  protected matchesForGroup_(groupId: number): AutocompleteMatch[] {
    return (this.result?.matches ?? [])
        .filter(
            match => (match.suggestionGroupId === groupId && !match.isHidden));
  }

  /**
   * @returns The list of side types to show.
   */
  protected sideTypes_(): SideType[] {
    return this.showSecondarySide_ ?
        [SideType.kDefaultPrimary, SideType.kSecondary] :
        [SideType.kDefaultPrimary];
  }

  /**
   * @returns The side type for the given suggestion group ID.
   */
  protected sideTypeForGroup_(groupId: number): SideType {
    return this.result?.suggestionGroupsMap[groupId]?.sideType ??
        SideType.kDefaultPrimary;
  }

  private computeShowSecondarySide_(): boolean {
    if (!this.canShowSecondarySide) {
      // Parent prohibits showing secondary side.
      return false;
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

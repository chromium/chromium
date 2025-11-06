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
import {RenderType, SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox_dropdown.css.js';
import {getHtml} from './searchbox_dropdown.html.js';
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

      /** Index of the selected match. */
      selectedMatchIndex: {
        type: Number,
        notify: true,
      },

      showThumbnail: {type: Boolean},

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
  accessor selectedMatchIndex: number = -1;
  accessor showThumbnail: boolean = false;
  private accessor showSecondarySide_: boolean = false;

  /** The list of selectable match elements. */
  private selectableMatchElements_: SearchboxMatchElement[] = [];

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

    this.onResultRepaint_();

    // Update the list of selectable match elements.
    this.selectableMatchElements_ =
        [...this.shadowRoot.querySelectorAll('cr-searchbox-match')];
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
    return this.updateComplete;
  }

  /** Selects the match at the given index. */
  selectIndex(index: number) {
    this.selectedMatchIndex = index;
    return this.updateComplete;
  }

  updateSelection(
      oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    // If the updated selection is a new match, remove any remaining selection
    // on the previously selected match.
    if (oldSelection.line !== selection.line) {
      const oldMatch = this.selectableMatchElements[this.selectedMatchIndex];
      if (oldMatch) {
        oldMatch.selection = selection;
      }
    }
    this.selectIndex(selection.line);
    const newMatch = this.selectableMatchElements[this.selectedMatchIndex];
    if (newMatch) {
      newMatch.selection = selection;
    }
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

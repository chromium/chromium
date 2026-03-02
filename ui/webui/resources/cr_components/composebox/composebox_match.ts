// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './composebox_match.css.js';
import {getHtml} from './composebox_match.html.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from './composebox_proxy.js';

const LINE_HEIGHT_PX = 24;
const MAX_DEEP_SEARCH_LINES = 2;

export interface ComposeboxMatchElement {
  $: {
    remove: HTMLElement,
    textContainer: HTMLElement,
  };
}

// Displays an autocomplete match
export class ComposeboxMatchElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-match';
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

      match: {type: Object},
      overrideClampLineNum: {
        type: Number,
        reflect: true,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       */
      matchIndex: {type: Number},

      toolMode: {
        type: Number,
        reflect: true,
      },

      removeButtonTitle_: {type: String},
    };
  }

  accessor match: AutocompleteMatch = createAutocompleteMatch();
  accessor overrideClampLineNum: number = -1;

  accessor matchIndex: number = -1;
  accessor toolMode: ToolMode = ToolMode.kUnspecified;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  protected accessor removeButtonTitle_: string =
      loadTimeData.getString('removeSuggestion');

  // Used for text clamping.
  private resizeObserver_: ResizeObserver|null = null;

  constructor() {
    super();
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    // Use mousedown to avoid clicks being swallowed by focusin.
    this.addEventListener('click', (event) => this.onMouseClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());

    // Prevent default mousedown behavior (e.g., focus) to avoid layout shifts
    // that could interfere with click events, especially for ZPS suggestions.
    this.addEventListener('mousedown', (event) => event.preventDefault());

    // Set up observer for responsive clamping.
    this.resizeObserver_ =
        new ResizeObserver(() => this.clampDeepSearchContents_());
    this.resizeObserver_.observe(this.$.textContainer);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.unobserve(this.$.textContainer);
    }
  }

  // Cannot render match content by modifying DOM until after `updated`.
  // Previously, `computeContent` was called in the `textContainer`
  // `html` file. This caused several Lit rendering errors, as logged by the
  // console.
  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('match')) {
      this.computeContents_();
    }
  }

  // This is needed since --webkit-box is deprecated and line-clamp does not
  // work in CSS without it.
  private clampDeepSearchContents_() {
    if (this.overrideClampLineNum > 0 &&
        this.toolMode !== ToolMode.kDeepSearch) {
      return;
    }

    // Get the suggestion textbox and update its contents
    // to be the full match text at first.
    const textContainer = this.$.textContainer;
    textContainer.textContent = this.match.contents;

    const clampLineNum = this.overrideClampLineNum > 0 ?
        this.overrideClampLineNum :
        MAX_DEEP_SEARCH_LINES;

    // See padding in window text container.
    const style = window.getComputedStyle(textContainer);
    const padding =
        parseFloat(style.paddingTop) + parseFloat(style.paddingBottom);

    // Max height must include padding since scrollHeight might.
    const textMaxHeight = LINE_HEIGHT_PX * clampLineNum + padding;

    // Exit if no need to elide text since is within max height
    // for x lines.
    if (textContainer.scrollHeight <= textMaxHeight) {
      return;
    }

    const words = this.match.contents.split(' ');
    while (words.length > 0) {
      words.pop();
      // Update the html textbox to the new truncated version.
      textContainer.textContent = words.join(' ') + '...';

      // Check against the height that includes padding.
      // Exit if no need to elide text since is within max height
      // for x lines.
      if (textContainer.scrollHeight <= textMaxHeight) {
        break;
      }
    }
  }

  protected computeContents_(): void {
    const textContainer = this.$.textContainer;
    textContainer.textContent = this.match.contents;
  }

  protected computeRemoveButtonAriaLabel_(): string {
    return this.match.removeButtonA11yLabel;
  }

  protected iconPath_(): string {
    return this.match.iconPath || '';
  }

  private onMatchFocusin_() {
    this.fire('match-focusin', {
      index: this.matchIndex,
    });
  }

  private onMouseClick_(e: MouseEvent) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    e.preventDefault();  // Prevents default browser action (navigation).

    this.searchboxHandler_.openAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl,
        /* are_matches_showing */ true, e.button || 0, e.altKey, e.ctrlKey,
        e.metaKey, e.shiftKey);

    this.fire('match-click');
  }

  protected onRemoveButtonClick_(e: MouseEvent) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.

    this.searchboxHandler_.deleteAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl);
  }

  protected onRemoveButtonMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-match': ComposeboxMatchElement;
  }
}

customElements.define(ComposeboxMatchElement.is, ComposeboxMatchElement);

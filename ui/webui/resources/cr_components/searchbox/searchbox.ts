// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_dropdown.js';
import './searchbox_icon.js';
import './searchbox_input.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox.css.js';
import {getHtml} from './searchbox.html.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';
import type {SearchboxMixinInterface} from './searchbox_mixin.js';
import {SearchboxMixin} from './searchbox_mixin.js';

export interface SearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const SearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

/** A real search box that behaves just like the Omnibox. */
export class SearchboxElement extends SearchboxElementBase implements
    SearchboxMixinInterface {
  static get is() {
    return 'cr-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties(): any {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor placeholderText: string = '';
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = false;

  protected callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerInterface;

  private autocompleteResultChangedListenerId_: number|null = null;

  constructor() {
    performance.mark('realbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming')) {
      this.useWebkitSearchIcons_ = this.searchboxChromeRefreshTheming;
    }
  }

  override firstUpdated() {
    performance.measure('realbox-creation', 'realbox-creation-start');
    this.initialInputScrollHeight = this.$.input.scrollHeight;
  }

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    if (!isEnabled) {
      return false;
    }

    if (!this.isInputEmpty()) {
      return false;
    }

    return true;
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  focusInput() {
    this.$.input.focus();
  }

  selectAll() {
    this.$.input.select();
  }

  //============================================================================
  // Callbacks
  //============================================================================

  //============================================================================
  // Event handlers
  //============================================================================

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e, /*is_composing=*/ false);
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  //============================================================================
  // Helpers
  //============================================================================

  protected computePlaceholderText_(placeholderText: string): string {
    if (placeholderText) {
      return placeholderText;
    }
    return this.i18n('searchBoxHint');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox': SearchboxElement;
  }
}

customElements.define(SearchboxElement.is, SearchboxElement);

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.js';
import '../icons_lit.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';
import {CrSearchFieldMixinLit} from '../cr_search_field/cr_search_field_mixin_lit.js';

import {getCss} from './cr_toolbar_search_field.css.js';
import {getHtml} from './cr_toolbar_search_field.html.js';

export interface CrToolbarSearchFieldElement {
  $: {
    icon: CrIconButtonElement,
    searchInput: HTMLInputElement,
    searchTerm: HTMLElement,
  };
}

const CrToolbarSearchFieldElementBase = CrSearchFieldMixinLit(CrLitElement);

export class CrToolbarSearchFieldElement extends
    CrToolbarSearchFieldElementBase {
  static get is() {
    return 'cr-toolbar-search-field';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      narrow: {
        type: Boolean,
        reflect: true,
      },

      showingSearch: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      disabled: {
        type: Boolean,
        reflect: true,
      },

      autofocus: {
        type: Boolean,
        reflect: true,
      },

      // When true, show a loading spinner to indicate that the backend is
      // processing the search. Will only show if the search field is open.
      spinnerActive: {
        type: Boolean,
        reflect: true,
      },

      searchFocused_: {
        type: Boolean,
        reflect: true,
      },

      iconOverride: {type: String},

      inputAriaDescription: {type: String},
    };
  }

  narrow: boolean = false;
  showingSearch: boolean = false;
  disabled: boolean = false;
  override autofocus: boolean = false;
  spinnerActive: boolean = false;
  private searchFocused_: boolean = false;
  iconOverride?: string;
  inputAriaDescription: string = '';

  override firstUpdated() {
    this.addEventListener('click', e => this.showSearch_(e));
  }

  override getSearchInput(): HTMLInputElement {
    return this.$.searchInput;
  }

  isSearchFocused(): boolean {
    return this.searchFocused_;
  }

  async showAndFocus() {
    this.showingSearch = true;
    await this.updateComplete;
    this.focus_();
  }

  protected onSearchTermNativeBeforeInput(e: InputEvent) {
    this.fire('search-term-native-before-input', {e});
  }

  override onSearchTermInput() {
    super.onSearchTermInput();
    this.showingSearch = this.hasSearchText || this.isSearchFocused();
  }

  protected onSearchTermNativeInput(e: InputEvent) {
    this.onSearchTermInput();
    this.fire('search-term-native-input', {e, inputValue: this.getValue()});
  }

  protected getIconTabIndex_(): number {
    return this.narrow && !this.hasSearchText ? 0 : -1;
  }

  protected getIconAriaHidden_(): string {
    return Boolean(!this.narrow || this.hasSearchText).toString();
  }

  protected shouldShowSpinner_(): boolean {
    return this.spinnerActive && this.showingSearch;
  }

  protected onSearchIconClicked_() {
    this.fire('search-icon-clicked');
  }

  private focus_() {
    this.getSearchInput().focus();
  }

  protected onInputFocus_() {
    this.searchFocused_ = true;
  }

  protected onInputBlur_() {
    this.searchFocused_ = false;
    if (!this.hasSearchText) {
      this.showingSearch = false;
    }
  }

  protected onSearchTermKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.showingSearch = false;
      this.setValue('');
      this.getSearchInput().blur();
    }
  }

  private async showSearch_(e: Event) {
    if (e.target !== this.shadowRoot!.querySelector('#clearSearch')) {
      this.showingSearch = true;
    }
    if (this.narrow) {
      await this.updateComplete; // Wait for input to become focusable.
      this.focus_();
    }
  }

  protected clearSearch_() {
    this.setValue('');
    this.focus_();
    this.spinnerActive = false;
    this.fire('search-term-cleared');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-search-field': CrToolbarSearchFieldElement;
  }
}

customElements.define(
    CrToolbarSearchFieldElement.is, CrToolbarSearchFieldElement);

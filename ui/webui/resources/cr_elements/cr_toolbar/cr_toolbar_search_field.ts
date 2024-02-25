// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.js';
import '../icons.html.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrSearchFieldMixinLit} from '../cr_search_field/cr_search_field_mixin_lit.js';

import {getCss} from './cr_toolbar_search_field.css.js';
import {getHtml} from './cr_toolbar_search_field.html.js';

export interface CrToolbarSearchFieldElement {
  $: {
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

      isSpinnerShown_: {type: Boolean},

      searchFocused_: {
        type: Boolean,
        reflect: true,
      },

      iconAriaHidden_: {type: String},
      iconTabIndex_: {type: Number},
    };
  }

  narrow: boolean;
  showingSearch: boolean = false;
  disabled: boolean = false;
  override autofocus: boolean = false;
  spinnerActive: boolean;
  protected isSpinnerShown_: boolean;
  private searchFocused_: boolean = false;
  protected iconAriaHidden_: string;
  protected iconTabIndex_: number;

  override firstUpdated() {
    this.addEventListener('click', e => this.showSearch_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('hasSearchText') ||
        changedProperties.has('narrow')) {
      this.iconAriaHidden_ =
          Boolean(!this.narrow || this.hasSearchText).toString();
      this.iconTabIndex_ = this.narrow && !this.hasSearchText ? 0 : -1;
    }

    if (changedProperties.has('spinnerActive') ||
        changedProperties.has('showingSearch')) {
      this.isSpinnerShown_ = this.spinnerActive && this.showingSearch;
    }
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

  override onSearchTermInput() {
    super.onSearchTermInput();
    this.showingSearch = this.hasSearchText || this.isSearchFocused();
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

  private showSearch_(e: Event) {
    if (e.target !== this.shadowRoot!.querySelector('#clearSearch')) {
      this.showingSearch = true;
    }
    if (this.narrow) {
      this.focus_();
    }
  }

  protected clearSearch_() {
    this.setValue('');
    this.focus_();
    this.spinnerActive = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-search-field': CrToolbarSearchFieldElement;
  }
}

customElements.define(
    CrToolbarSearchFieldElement.is, CrToolbarSearchFieldElement);

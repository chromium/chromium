// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.js';
import '../icons_lit.html.js';
import './cr_toolbar_search_field.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_toolbar.css.js';
import {getHtml} from './cr_toolbar.html.js';
import type {CrToolbarSearchFieldElement} from './cr_toolbar_search_field.js';

export interface CrToolbarElement {
  $: {
    search: CrToolbarSearchFieldElement,
  };
}

export class CrToolbarElement extends CrLitElement {
  static get is() {
    return 'cr-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Name to display in the toolbar, in titlecase.
      pageName: {type: String},

      // Prompt text to display in the search field.
      searchPrompt: {type: String},

      // Tooltip to display on the clear search button.
      clearLabel: {type: String},

      // Tooltip to display on the menu button.
      menuLabel: {type: String},

      // Value is proxied through to cr-toolbar-search-field. When true,
      // the search field will show a processing spinner.
      spinnerActive: {type: Boolean},

      // Controls whether the menu button is shown at the start of the menu.
      showMenu: {type: Boolean},

      // Controls whether the search field is shown.
      showSearch: {type: Boolean},

      // Controls whether the search field is autofocused.
      autofocus: {
        type: Boolean,
        reflect: true,
      },

      // True when the toolbar is displaying in narrow mode.
      narrow: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      /**
       * The threshold at which the toolbar will change from normal to narrow
       * mode, in px.
       */
      narrowThreshold: {
        type: Number,
      },

      alwaysShowLogo: {
        type: Boolean,
        reflect: true,
      },

      showingSearch_: {
        type: Boolean,
        reflect: true,
      },

      searchIconOverride: {type: String},
      searchInputAriaDescription: {type: String},
    };
  }

  pageName: string = '';
  searchPrompt: string = '';
  clearLabel: string = '';
  menuLabel?: string;
  spinnerActive: boolean = false;
  showMenu: boolean = false;
  showSearch: boolean = true;
  override autofocus: boolean = false;
  narrow: boolean = false;
  narrowThreshold: number = 900;
  alwaysShowLogo: boolean = false;
  protected showingSearch_: boolean = false;
  searchIconOverride?: string;
  searchInputAriaDescription: string = '';
  private narrowQuery_: MediaQueryList|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('narrowThreshold')) {
      this.narrowQuery_ =
          window.matchMedia(`(max-width: ${this.narrowThreshold}px)`);
      this.narrow = this.narrowQuery_.matches;
      this.narrowQuery_.addListener(() => this.onQueryChanged_());
    }
  }

  getSearchField(): CrToolbarSearchFieldElement {
    return this.$.search;
  }

  protected onMenuClick_() {
    this.fire('cr-toolbar-menu-click');
  }

  async focusMenuButton() {
    assert(this.showMenu);
    // Wait for rendering to finish to ensure menuButton exists on the DOM.
    await this.updateComplete;
    const menuButton =
        this.shadowRoot!.querySelector<HTMLElement>('#menuButton');
    assert(!!menuButton);
    menuButton.focus();
  }

  isMenuFocused(): boolean {
    return !!this.shadowRoot!.activeElement &&
        this.shadowRoot!.activeElement.id === 'menuButton';
  }

  protected onShowingSearchChanged_(e: CustomEvent<{value: boolean}>) {
    this.showingSearch_ = e.detail.value;
  }

  private onQueryChanged_() {
    assert(this.narrowQuery_);
    this.narrow = this.narrowQuery_.matches;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar': CrToolbarElement;
  }
}

customElements.define(CrToolbarElement.is, CrToolbarElement);

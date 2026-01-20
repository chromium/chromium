// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import './contextual_action_menu.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {GlifAnimationState} from './context_menu_entrypoint.js';
import {getCss} from './contextual_entrypoint_button.css.js';
import {getHtml} from './contextual_entrypoint_button.html.js';
import type {ContextualActionMenuElement} from './contextual_action_menu.js';

/** The string value of the tall bottom context layout mode. */
const TALL_BOTTOM_CONTEXT_LAYOUT_MODE = 'TallBottomContext';

export interface ContextualEntrypointButtonElement {
  $: {
    menu: ContextualActionMenuElement,
  };
}

const ContextualEntrypointButtonElementBase = I18nMixinLit(CrLitElement);

export class ContextualEntrypointButtonElement extends
    ContextualEntrypointButtonElementBase {
  static get is() {
    return 'cr-composebox-contextual-entrypoint-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this as any)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      inputsDisabled: {type: Boolean},
      fileNum: {type: Number},
      showContextMenuDescription: {type: Boolean},
      hasImageFiles: {
        reflect: true,
        type: Boolean,
      },
      hideEntrypointButton: {type: Boolean},
      disabledTabIds: {type: Object},
      tabSuggestions: {type: Array},
      entrypointName: {type: String},
      searchboxLayoutMode: {type: String},
      glifAnimationState: {type: String, reflect: true},

      // =========================================================================
      // Protected properties
      // =========================================================================
      enableMultiTabSelection_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor inputsDisabled: boolean = false;
  accessor fileNum: number = 0;
  accessor showContextMenuDescription: boolean = false;
  accessor hasImageFiles: boolean = false;
  accessor hideEntrypointButton: boolean = false;
  accessor disabledTabIds: Map<number, UnguessableToken> = new Map();
  accessor tabSuggestions: TabInfo[] = [];
  accessor entrypointName: string = '';
  accessor searchboxLayoutMode: string = '';
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;

  protected accessor enableMultiTabSelection_: boolean =
      loadTimeData.getBoolean('composeboxContextMenuEnableMultiTabSelection');
  private metricsSource_: string = loadTimeData.getString('composeboxSource');

  constructor() {
    super();
  }

  openMenuForMultiSelection() {
    if (this.enableMultiTabSelection_ &&
        this.searchboxLayoutMode !== TALL_BOTTOM_CONTEXT_LAYOUT_MODE) {
      this.updateComplete.then(this.showMenuAtEntrypoint_.bind(this));
    }
  }

  closeMenu() {
    this.$.menu.close();
  }

  protected onEntrypointClick_(e: Event) {
    e.stopPropagation();

    const metricName =
        'ContextualSearch.ContextMenuEntry.Clicked.' + this.metricsSource_;
    chrome.metricsPrivate.recordBoolean(metricName, true);
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    this.fire('context-menu-entrypoint-click', {
      x: entrypoint.getBoundingClientRect().left,
      y: entrypoint.getBoundingClientRect().bottom,
    });
    if (this.entrypointName !== 'Omnibox') {
      this.showMenuAtEntrypoint_();
    }
  }

  protected onAnimationEnd_(e: AnimationEvent, animationName: string) {
    if (e.animationName === animationName) {
      this.glifAnimationState = GlifAnimationState.FINISHED;
    }
  }

  protected onMenuClose_() {
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    entrypoint.classList.remove('menu-open');
    this.fire('context-menu-closed');
  }

  private showMenuAtEntrypoint_() {
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    entrypoint?.classList.add('menu-open');
    this.$.menu.showAt(entrypoint);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-contextual-entrypoint-button':
        ContextualEntrypointButtonElement;
  }
}

customElements.define(
    ContextualEntrypointButtonElement.is,
    ContextualEntrypointButtonElement);

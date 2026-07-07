// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './contextual_action_menu.js';
import './contextual_entrypoint_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {GlifAnimationState} from './common.js';
import type {ContextualActionMenuElement} from './contextual_action_menu.js';
import {getCss} from './contextual_entrypoint_and_menu.css.js';
import {getHtml} from './contextual_entrypoint_and_menu.html.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';

export interface ContextualEntrypointAndMenuElement {
  $: {
    menu: ContextualActionMenuElement,
  };
}

interface EntrypointElements {
  entrypointButton?: ContextualEntrypointButtonElement|null;
  entrypoint?: HTMLElement|null;
}

const ContextualEntrypointAndMenuElementBase = I18nMixinLit(CrLitElement);

export class ContextualEntrypointAndMenuElement extends
    ContextualEntrypointAndMenuElementBase {
  static get is() {
    return 'cr-composebox-contextual-entrypoint-and-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      fileNum: {type: Number},
      nonTabFileNum: {type: Number},
      showContextMenuDescription: {type: Boolean},
      smartTabSharingActive: {type: Boolean},
      smartTabSharingVisible: {type: Boolean},
      hasImageFiles: {
        reflect: true,
        type: Boolean,
      },
      disabledTabIds: {type: Object},
      aimThreadRestoredTabs: {type: Array},
      tabSuggestions: {type: Array},
      inputState: {type: Object},
      tabSuggestionsLoading: {type: Boolean},
      tabSuggestionsHasLoaded: {type: Boolean},
      glifAnimationState: {type: String},
      searchboxLayoutMode: {type: String},
      uploadButtonDisabled: {type: Boolean},
      disableAutoReposition: {type: Boolean},
      isSidePanel: {type: Boolean},
      usePecApi: {type: Boolean},
      energyEffectAnimationEnabled: {type: Boolean, reflect: true},
      disableFallbackGlifAnimation: {type: Boolean},
      recentTabId: {type: Number},
      shareTabsFlyoutOpen: {type: Boolean},

      // =========================================================================
      // Protected properties
      // =========================================================================
      enableMultiTabSelection_: {
        reflect: true,
        type: Boolean,
      },
      sharedTabs: {type: Array},
    };
  }

  accessor fileNum: number = 0;
  accessor nonTabFileNum: number = 0;
  accessor showContextMenuDescription: boolean = false;
  accessor smartTabSharingActive: boolean = false;
  accessor smartTabSharingVisible: boolean = false;
  accessor disabledTabIds: Map<number, UnguessableToken> = new Map();
  accessor aimThreadRestoredTabs: TabInfo[] = [];
  accessor tabSuggestions: TabInfo[] = [];
  accessor inputState: InputState|null = null;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor uploadButtonDisabled: boolean = false;
  accessor sharedTabs: TabInfo[] = [];
  accessor recentTabId: number|null = null;
  accessor shareTabsFlyoutOpen: boolean = false;
  accessor tabSuggestionsLoading: boolean = false;
  accessor tabSuggestionsHasLoaded: boolean = false;
  menuOpenDelayMs: number = 200;

  private openTimeoutId_: number|null = null;

  accessor hasImageFiles: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor disableAutoReposition: boolean = false;
  accessor usePecApi: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor isSidePanel: boolean = false;
  accessor disableFallbackGlifAnimation: boolean = false;

  protected accessor enableMultiTabSelection_: boolean =
      loadTimeData.getBoolean('composeboxContextMenuEnableMultiTabSelection');

  // TODO(crbug.com/499310611): Explore avoiding/removing this local property.
  private shouldOpenMenuForMultiSelection_: boolean = false;

  openMenuForMultiSelection() {
    if (this.enableMultiTabSelection_) {
      this.shouldOpenMenuForMultiSelection_ = true;
      this.requestUpdate();
    }
  }

  private getEntrypointElements_(): EntrypointElements {
    const entrypointButton =
        this.shadowRoot?.querySelector<ContextualEntrypointButtonElement>(
            '#entrypointButton');
    const entrypoint =
        entrypointButton?.shadowRoot?.querySelector<HTMLElement>('#entrypoint');
    return {entrypointButton, entrypoint};
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.openTimeoutId_ !== null) {
      window.clearTimeout(this.openTimeoutId_);
      this.openTimeoutId_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('tabSuggestionsLoading')) {
      if (!this.tabSuggestionsLoading && this.openTimeoutId_ !== null) {
        window.clearTimeout(this.openTimeoutId_);
        this.openTimeoutId_ = null;
        this.$.menu.updateComplete.then(() => {
          this.showMenuAtEntrypoint_();
        });
      }
    }

    if (this.shouldOpenMenuForMultiSelection_) {
      const {entrypointButton, entrypoint} = this.getEntrypointElements_();
      // Clear the flag if the entrypoint is fully or partially rendered, or if
      // inputState is truthy (valid) but the entrypoint is not rendered
      // (ex: because `hasAllowedInputs()` returned false based on
      // `usePecApi` or disabled inputs) to prevent a delayed pop open.
      if (entrypoint || entrypointButton || this.inputState) {
        this.shouldOpenMenuForMultiSelection_ = false;
      }

      if (entrypoint) {
        this.showMenuAtEntrypoint_();
      } else if (entrypointButton) {
        // Button is in the DOM but hasn't finished rendering its own shadow
        // root.
        entrypointButton.updateComplete.then(() => {
          this.showMenuAtEntrypoint_();
        });
      }
    }
  }

  closeMenu() {
    const menu =
        this.shadowRoot.querySelector<ContextualActionMenuElement>('#menu');
    if (menu) {
      menu.close();
    }
  }

  protected onMenuClose_() {
    const {entrypointButton} = this.getEntrypointElements_();
    if (entrypointButton) {
      entrypointButton.classList.remove('menu-open');
    }
    this.fire('context-menu-closed');
  }

  protected onContextMenuEntrypointClick_() {
    if (this.openTimeoutId_ !== null) {
      return;
    }
    if (!this.tabSuggestionsHasLoaded && !this.tabSuggestionsLoading) {
      this.tabSuggestionsLoading = true;
      this.fire('request-tab-suggestions-load');
    }
    if (this.tabSuggestionsLoading) {
      this.openTimeoutId_ = window.setTimeout(() => {
        this.openTimeoutId_ = null;
        this.showMenuAtEntrypoint_();
      }, this.menuOpenDelayMs);
    } else {
      this.showMenuAtEntrypoint_();
    }
  }

  protected onContextMenuEntrypointHover_() {
    this.fire('context-menu-entrypoint-hover');
  }

  private async showMenuAtEntrypoint_() {
    const {entrypointButton, entrypoint} = this.getEntrypointElements_();
    if (entrypointButton && entrypoint) {
      entrypointButton.classList.add('menu-open');
      await this.updateComplete;
      await this.$.menu.updateComplete;
      this.$.menu.showAt(entrypoint);
      this.fire('context-menu-opened');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-contextual-entrypoint-and-menu':
        ContextualEntrypointAndMenuElement;
  }
}

customElements.define(
    ContextualEntrypointAndMenuElement.is,
    ContextualEntrypointAndMenuElement);

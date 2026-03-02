// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './contextual_action_menu.js';
import './contextual_entrypoint_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {GlifAnimationState} from './common.js';
import type {ContextualActionMenuElement} from './contextual_action_menu.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';
import {getCss} from './contextual_entrypoint_and_menu.css.js';
import {getHtml} from './contextual_entrypoint_and_menu.html.js';

export interface ContextualEntrypointAndMenuElement {
  $: {
    entrypointButton: ContextualEntrypointButtonElement,
    menu: ContextualActionMenuElement,
  };
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
    return getHtml.bind(this as any)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      fileNum: {type: Number},
      showContextMenuDescription: {type: Boolean},
      hasImageFiles: {
        reflect: true,
        type: Boolean,
      },
      disabledTabIds: {type: Object},
      tabSuggestions: {type: Array},
      inputState: {type: Object},
      glifAnimationState: {type: String, reflect: true},
      inCreateImageMode: {type: Boolean},
      searchboxLayoutMode: {type: String},
      uploadButtonDisabled: {type: Boolean},

      // =========================================================================
      // Protected properties
      // =========================================================================
      enableMultiTabSelection_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor fileNum: number = 0;
  accessor showContextMenuDescription: boolean = false;
  accessor disabledTabIds: Map<number, UnguessableToken> = new Map();
  accessor tabSuggestions: TabInfo[] = [];
  accessor inputState: InputState|null = null;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor uploadButtonDisabled: boolean = false;

  // TODO(crbug.com/476467436): Remove these properties once the
  // `cr-composebox-context-menu-entrypoint` is removed.
  accessor inCreateImageMode: boolean = false;
  accessor hasImageFiles: boolean = false;
  accessor searchboxLayoutMode: string = '';

  protected accessor enableMultiTabSelection_: boolean =
      loadTimeData.getBoolean('composeboxContextMenuEnableMultiTabSelection');

  constructor() {
    super();
  }

  openMenuForMultiSelection() {
    if (this.enableMultiTabSelection_) {
      this.updateComplete.then(this.showMenuAtEntrypoint_.bind(this));
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
    this.$.entrypointButton.classList.remove('menu-open');
    this.fire('context-menu-closed');
  }

  protected showMenuAtEntrypoint_() {
    this.$.entrypointButton.classList.add('menu-open');
    const entrypoint =
        this.$.entrypointButton.shadowRoot.querySelector<HTMLElement>(
            '#entrypoint');
    assert(entrypoint);
    this.fire('context-menu-opened');
    this.$.menu.showAt(entrypoint);
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

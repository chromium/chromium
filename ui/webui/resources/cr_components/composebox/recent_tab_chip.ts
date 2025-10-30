// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './recent_tab_chip.css.js';
import {getHtml} from './recent_tab_chip.html.js';

const RecentTabChipBase = I18nMixinLit(CrLitElement);

export class RecentTabChipElement extends RecentTabChipBase {
  static get is() {
    return 'composebox-recent-tab-chip';
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
      inputsDisabled: {type: Boolean},
      recentTab: {type: Object},
    };
  }

  accessor inputsDisabled: boolean = false;
  accessor recentTab: TabInfo|undefined = undefined;

  protected addTabContext_(e: Event) {
    e.stopPropagation();
    if (!this.recentTab || this.inputsDisabled) {
      return;
    }
    this.fire('add-tab-context', {
      id: this.recentTab.tabId,
      title: this.recentTab.title,
      url: this.recentTab.url,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-recent-tab-chip': RecentTabChipElement;
  }
}

customElements.define(RecentTabChipElement.is, RecentTabChipElement);

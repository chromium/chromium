// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './composebox_tab_favicon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
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
      recentTab: {type: Object},
    };
  }

  accessor recentTab: TabInfo|undefined = undefined;

  private delayTabUploads_: boolean =
      loadTimeData.getBoolean('addTabUploadDelayOnRecentTabChipClick');

  protected addTabContext_(e: Event) {
    e.stopPropagation();
    assert(this.recentTab);

    this.fire('add-tab-context', {
      id: this.recentTab.tabId,
      title: this.recentTab.title,
      url: this.recentTab.url,
      delayUpload: this.delayTabUploads_,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-recent-tab-chip': RecentTabChipElement;
  }
}

customElements.define(RecentTabChipElement.is, RecentTabChipElement);

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {htmlEscape} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {TabAttachmentSource} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {recordContextAdditionMethod} from './common.js';
import {getCss} from './current_tab_chip.css.js';
import {getHtml} from './current_tab_chip.html.js';

const CurrentTabChipBase = I18nMixinLit(CrLitElement);

export class CurrentTabChipElement extends CurrentTabChipBase {
  static get is() {
    return 'composebox-current-tab-chip';
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
      currentTab: {type: Object},
      hasVirtualFocus: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor currentTab: TabInfo|undefined = undefined;
  accessor hasVirtualFocus: boolean = false;

  private composeboxSource_: string =
      loadTimeData.getString('composeboxSource');

  protected getCurrentTabChipTitle_(): string {
    if (!this.currentTab) {
      return '';
    }
    const url = new URL(this.currentTab.url);
    const domain = url.hostname.replace(/^www\./, '');
    // Escape the title and domain as they are passed as arguments to i18n(),
    // which uses `parseHtmlSubset` to sanitize the localized string. If the
    // title contains restricted HTML tags (like <style>), parseHtmlSubset will
    // throw an error.
    return `${htmlEscape(this.currentTab.title)} - ${htmlEscape(domain)}`;
  }

  /**
   * Returns the label screen readers narrate for this chip, the same as its
   * button's aria-label.
   */
  getA11yLabel(): string {
    if (!this.currentTab) {
      return '';
    }
    return this.i18n('askAboutTabAriaLabel', this.getCurrentTabChipTitle_());
  }

  protected onCurrentTabButtonClick_(e: Event) {
    e.stopPropagation();
    assert(this.currentTab);

    chrome.histograms.recordUserAction(
        `ContextualSearch.CurrentTabChipClick.${this.composeboxSource_}`);

    this.fire('add-tab-context', {
      id: this.currentTab.tabId,
      title: this.currentTab.title,
      url: this.currentTab.url,
      delayUpload: false,
      origin: TabAttachmentSource.kCurrentTabChip,
    });
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CURRENT_TAB_CHIP, this.composeboxSource_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-current-tab-chip': CurrentTabChipElement;
  }
}

customElements.define(CurrentTabChipElement.is, CurrentTabChipElement);

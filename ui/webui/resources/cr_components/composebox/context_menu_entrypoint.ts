// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './context_menu_entrypoint.css.js';
import {getHtml} from './context_menu_entrypoint.html.js';


/** The width of the dropdown menu in pixels. */
const MENU_WIDTH_PX = 190;

export interface ContextMenuEntrypointElement {
  $: {
    menu: CrActionMenuElement,
  };
}

const ContextMenuEntrypointElementBase = I18nMixinLit(CrLitElement);

export class ContextMenuEntrypointElement extends
    ContextMenuEntrypointElementBase {
  static get is() {
    return 'composebox-context-menu-entrypoint';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      inputsDisabled: {type: Boolean},
      fileNum: {type: Number},
      showContextMenuDescription: {type: Boolean},
      inCreateImageMode: {
        reflect: true,
        type: Boolean,
      },
      hasImageFiles: {
        reflect: true,
        type: Boolean,
      },
      disabledTabIds: {type: Object},
      tabSuggestions_: {type: Array},
      tabPreviewUrl_: {type: String},
      tabPreviewsEnabled_: {type: Boolean},
      showDeepSearch_: {
        reflect: true,
        type: Boolean,
      },
      showCreateImage_: {
        reflect: true,
        type: Boolean,
      },
      entrypointName: {type: String},
    };
  }

  accessor inputsDisabled: boolean = false;
  accessor fileNum: number = 0;
  accessor showContextMenuDescription: boolean = false;
  accessor inCreateImageMode: boolean = false;
  accessor hasImageFiles: boolean = false;
  accessor disabledTabIds: Set<number> = new Set();
  accessor entrypointName: string = '';
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor tabPreviewUrl_: string = '';
  protected accessor tabPreviewsEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuTabPreviews');
  protected accessor showDeepSearch_: boolean =
      loadTimeData.getBoolean('composeboxShowDeepSearchButton');
  protected accessor showCreateImage_: boolean =
      loadTimeData.getBoolean('composeboxShowCreateImageButton');
  protected maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');

  constructor() {
    super();
  }

  // Checks if the image upload item in the context menu should be disabled.
  protected get imageUploadDisabled_(): boolean {
    return this.fileNum >= this.maxFileCount_ ||
        (this.inCreateImageMode && this.hasImageFiles);
  }

  // Checks if the file upload item in the context menu should be disabled.
  protected get fileUploadDisabled_(): boolean {
    return this.inCreateImageMode || this.fileNum >= this.maxFileCount_;
  }

  // Checks if the deep search item in the context menu should be disabled.
  protected get deepSearchDisabled_(): boolean {
    return this.inCreateImageMode || this.fileNum === 1 || this.fileNum > 1;
  }

  // Checks if the create image item in the context menu should be disabled.
  protected get createImageDisabled_(): boolean {
    return this.fileNum > 1 || ((this.fileNum === 1) && !this.hasImageFiles);
  }

  // Checks if a tab item in the context menu should be disabled.
  protected isTabDisabled_(tab: TabInfo): boolean {
    return this.inCreateImageMode || this.fileNum >= this.maxFileCount_ ||
        this.disabledTabIds.has(tab.tabId);
  }

  protected onEntrypointClick_() {
    const metricName =
        'NewTabPage.' + this.entrypointName + '.ContextMenuEntry.Clicked';
    chrome.metricsPrivate.recordBoolean(metricName, true);
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    this.$.menu.showAt(entrypoint, {
      top: entrypoint.getBoundingClientRect().bottom,
      width: MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment['AFTER_START'],
    });
  }

  protected addTabContext_(e: Event) {
    e.stopPropagation();

    const tabElement = e.currentTarget! as HTMLButtonElement;
    const tabInfo = this.tabSuggestions_[Number(tabElement.dataset['index'])];

    assert(tabInfo);

    this.fire('add-tab-context', {
      id: tabInfo.tabId,
      title: tabInfo.title,
      url: tabInfo.url,
    });
    this.$.menu.close();
  }

  protected onTabPointerenter_(e: Event) {
    if (!this.tabPreviewsEnabled_) {
      return;
    }

    const tabElement = e.currentTarget! as HTMLElement;
    const tabInfo = this.tabSuggestions_[Number(tabElement.dataset['index'])];
    assert(tabInfo);

    // Clear the preview URL before fetching the new one to make sure an old
    // or incorrect preview doesn't show while the new one is loading.
    this.tabPreviewUrl_ = '';
    this.fire('get-tab-preview', {
      tabId: tabInfo.tabId,
      onPreviewFetched: (previewDataUrl: string) => {
        this.tabPreviewUrl_ = previewDataUrl;
      },
    });
  }

  protected shouldShowTabPreview_(): boolean {
    return this.tabPreviewsEnabled_ && this.tabPreviewUrl_ !== '';
  }

  protected openImageUpload_() {
    this.fire('open-image-upload');
    this.$.menu.close();
  }

  protected openFileUpload_() {
    this.fire('open-file-upload');
    this.$.menu.close();
  }

  protected onDeepSearchClick_() {
    this.fire('deep-search-click');
    this.$.menu.close();
  }

  protected onCreateImageClick_() {
    this.fire('create-image-click');
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-context-menu-entrypoint': ContextMenuEntrypointElement;
  }
}

customElements.define(
    ContextMenuEntrypointElement.is, ContextMenuEntrypointElement);

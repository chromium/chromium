// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {recordContextAdditionMethod} from './common.js';
import {getCss} from './context_menu_entrypoint.css.js';
import {getHtml} from './context_menu_entrypoint.html.js';


/** The width of the dropdown menu in pixels. */
const MENU_WIDTH_PX = 190;
/** The string value of the tall bottom context layout mode. */
const TALL_BOTTOM_CONTEXT_LAYOUT_MODE = 'TallBottomContext';

export interface ContextMenuEntrypointElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export enum GlifAnimationState {
  INELIGIBLE = 'ineligible',
  SPINNER_ONLY = 'spinner-only',
  STARTED = 'started',
  FINISHED = 'finished',
}

const ContextMenuEntrypointElementBase = I18nMixinLit(CrLitElement);

export class ContextMenuEntrypointElement extends
    ContextMenuEntrypointElementBase {
  static get is() {
    return 'cr-composebox-context-menu-entrypoint';
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
      pdfUploadEnabled_: {
        type: Boolean,
      },
    };
  }

  accessor inputsDisabled: boolean = false;
  accessor fileNum: number = 0;
  accessor showContextMenuDescription: boolean = false;
  accessor inCreateImageMode: boolean = false;
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
  protected accessor tabPreviewUrl_: string = '';
  protected accessor tabPreviewsEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuTabPreviews');
  protected accessor showDeepSearch_: boolean =
      loadTimeData.getBoolean('composeboxShowDeepSearchButton');
  protected accessor showCreateImage_: boolean =
      loadTimeData.getBoolean('composeboxShowCreateImageButton');
  protected accessor pdfUploadEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowPdfUpload');
  protected maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
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
    return this.inCreateImageMode || this.fileNum > 1 ||
        ((this.fileNum === 1) && !this.hasImageFiles);
  }

  // Checks if a tab item in the context menu should be disabled.
  protected isTabDisabled_(tab: TabInfo): boolean {
    const noNewContextAllowed =
        this.inCreateImageMode || this.fileNum >= this.maxFileCount_;
    const isTabInContext = this.disabledTabIds.has(tab.tabId);
    // If multi-tab selection is enabled, we only want to disable a tab if
    // no more context can be added and the tab has not yet been added as
    // context already. Otherwise, don't disable the tab, since we want to allow
    // users to unselect the tab, and remove it from the context.
    if (this.enableMultiTabSelection_) {
      return noNewContextAllowed && !isTabInContext;
    }
    return noNewContextAllowed || isTabInContext;
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

  protected onTabClick_(e: Event) {
    e.stopPropagation();

    const tabElement = e.currentTarget! as HTMLButtonElement;
    const tabInfo = this.tabSuggestions[Number(tabElement.dataset['index'])];

    assert(tabInfo);

    if (this.enableMultiTabSelection_ &&
        this.disabledTabIds.has(tabInfo.tabId)) {
      this.deleteTabContext_(this.disabledTabIds.get(tabInfo.tabId)!);
      return;
    }
    this.addTabContext_(tabInfo);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.metricsSource_);
  }

  protected deleteTabContext_(uuid: UnguessableToken) {
    this.fire('delete-tab-context', {uuid: uuid});
    if (this.searchboxLayoutMode === TALL_BOTTOM_CONTEXT_LAYOUT_MODE) {
      this.$.menu.close();
    }
  }


  protected addTabContext_(tabInfo: TabInfo) {
    this.fire('add-tab-context', {
      id: tabInfo.tabId,
      title: tabInfo.title,
      url: tabInfo.url,
      delayUpload: false,
    });
    if (!this.enableMultiTabSelection_ || this.entrypointName === 'Realbox' ||
        this.searchboxLayoutMode === TALL_BOTTOM_CONTEXT_LAYOUT_MODE) {
      this.$.menu.close();
    }
  }

  protected onTabPointerenter_(e: Event) {
    if (!this.tabPreviewsEnabled_) {
      return;
    }

    const tabElement = e.currentTarget! as HTMLElement;
    const tabInfo = this.tabSuggestions[Number(tabElement.dataset['index'])];
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
  }

  private showMenuAtEntrypoint_() {
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    entrypoint?.classList.add('menu-open');
    this.$.menu.showAt(entrypoint, {
      top: entrypoint.getBoundingClientRect().bottom,
      width: MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment['AFTER_START'],
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-context-menu-entrypoint': ContextMenuEntrypointElement;
  }
}

customElements.define(
    ContextMenuEntrypointElement.is, ContextMenuEntrypointElement);

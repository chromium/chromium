// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getLoadTimeBoolean, recordContextAdditionMethod, TabUploadOrigin} from './common.js';
import {getCss} from './contextual_action_menu.css.js';
import {getHtml} from './contextual_action_menu.html.js';
import {WindowProxy} from './window_proxy.js';

/** The width of the dropdown menu in pixels. */
const MENU_WIDTH_PX = 190;

const SHARE_TABS_MENU_WIDTH_PX = 320;
const SHARE_TABS_FLYOUT_CLOSE_DELAY_MS = 300;

export interface ContextualActionMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

const ContextualActionMenuElementBase = I18nMixinLit(CrLitElement);

export class ContextualActionMenuElement extends
    ContextualActionMenuElementBase {
  static get is() {
    return 'cr-composebox-contextual-action-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this as any)();
  }

  static override get properties() {
    return {
      fileNum: {type: Number},
      disabledTabIds: {type: Object},
      tabSuggestions: {type: Array},
      inputState: {type: Object},
      smartTabSharingActive: {type: Boolean},
      enableMultiTabSelection_: {
        reflect: true,
        type: Boolean,
      },
      tabPreviewUrl_: {type: String},
      tabPreviewsEnabled_: {type: Boolean},
      showContextMenuHeaders_: {type: Boolean},
      smartTabSharingVisible_: {type: Boolean},
      disableAutoReposition: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      shareTabsFlyoutOpen_: {type: Boolean},
    };
  }

  accessor fileNum: number = 0;
  accessor disabledTabIds: Map<number, UnguessableToken> = new Map();
  accessor tabSuggestions: TabInfo[] = [];
  accessor inputState: InputState|null = null;
  accessor smartTabSharingActive: boolean = false;
  accessor disableAutoReposition: boolean = false;

  protected accessor enableMultiTabSelection_: boolean =
      loadTimeData.getBoolean('composeboxContextMenuEnableMultiTabSelection');
  protected accessor tabPreviewUrl_: string = '';
  protected accessor tabPreviewsEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuTabPreviews');
  protected maxFileCount_: number =
      loadTimeData.getInteger('composeboxFileMaxCount');
  private metricsSource_: string = loadTimeData.getString('composeboxSource');
  protected accessor showContextMenuHeaders_: boolean =
      loadTimeData.getBoolean('ShowContextMenuHeaders');
  protected accessor smartTabSharingVisible_: boolean =
      getLoadTimeBoolean('composeboxSmartTabSharingVisible', false);
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      getLoadTimeBoolean('contextManagementInComposeboxEnabled', false);
  protected accessor shareTabsFlyoutOpen_: boolean = false;

  private closeTimer_: number|null = null;
  private pointerOverTrigger_: boolean = false;
  private pointerOverFlyout_: boolean = false;

  protected get supportedTools_(): Map<ToolMode, {
    icon: string,
  }> {
    return new Map([
      [
        ToolMode.kImageGen,
        {
          icon: 'composebox:nanoBanana',
        },
      ],
      [
        ToolMode.kDeepSearch,
        {
          icon: 'composebox:deepSearch',
        },
      ],
      [
        ToolMode.kCanvas,
        {
          icon: 'composebox:canvas',
        },
      ],
    ]);
  }

  protected get supportedModels_(): Map<ModelMode, {
    icon: string,
  }> {
    const thinkingIcon =
        (loadTimeData.getBoolean('thinkingModelIconUpdate') &&
         this.inputState &&
         this.inputState.allowedModels.includes(ModelMode.kGeminiProNoGenUi)) ?
        'composebox:astrophotographyMode' :
        'composebox:thinkingModel';
    return new Map([
      [
        ModelMode.kGeminiRegular,
        {
          icon: 'composebox:regularModel',
        },
      ],
      [
        ModelMode.kGeminiProAutoroute,
        {
          icon: 'composebox:autoModel',
        },
      ],
      [
        ModelMode.kGeminiPro,
        {
          icon: thinkingIcon,
        },
      ],
      [
        ModelMode.kGeminiProNoGenUi,
        {
          icon: 'composebox:thinkingModel',
        },
      ],
    ]);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resetShareTabsFlyout_();
  }

  get open(): boolean {
    return this.$.menu.open;
  }

  close() {
    this.$.menu.close();
  }

  getDialog(): HTMLDialogElement {
    return this.$.menu.getDialog();
  }

  private onWindowBlur_ = this.close.bind(this);

  showAt(anchor: HTMLElement) {
    this.$.menu.showAt(anchor, {
      width: this.contextManagementInComposeboxEnabled_ ? SHARE_TABS_MENU_WIDTH_PX : MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
    window.addEventListener('blur', this.onWindowBlur_);
  }

  protected isToolAllowed_(tool: ToolMode): boolean {
    if (!this.inputState) {
      return false;
    }
    return this.inputState.allowedTools.includes(tool);
  }

  protected isToolDisabled_(tool: ToolMode): boolean {
    if (!this.inputState) {
      return true;
    }
    return this.inputState.disabledTools.includes(tool);
  }

  protected isModelAllowed_(model: ModelMode): boolean {
    if (!this.inputState) {
      return false;
    }
    return this.inputState.allowedModels.includes(model);
  }

  protected isModelDisabled_(model: ModelMode): boolean {
    if (!this.inputState) {
      return true;
    }
    return this.inputState.disabledModels.includes(model);
  }

  protected isModelActive_(model: ModelMode): boolean {
    if (!this.inputState) {
      return false;
    }
    return this.inputState.activeModel === model;
  }

  protected getToolLabel_(tool: ToolMode): string {
    if (this.inputState) {
      const config = this.inputState.toolConfigs.find(c => c.tool === tool);
      if (config && config.menuLabel) {
        return config.menuLabel;
      }
    }
    switch (tool) {
      case ToolMode.kDeepSearch:
        return this.i18n('deepSearch');
      case ToolMode.kImageGen:
        return this.i18n('createImages');
      case ToolMode.kCanvas:
        return this.i18n('canvas');
      default:
        return '';
    }
  }

  protected getModelLabel_(model: ModelMode): string {
    if (this.inputState) {
      const config = this.inputState.modelConfigs.find(c => c.model === model);
      if (config && config.menuLabel) {
        return config.menuLabel;
      }
    }
    switch (model) {
      // We don't have a string for the regular model in the client code.
      case ModelMode.kGeminiRegular:
        return '';
      case ModelMode.kGeminiProAutoroute:
        return this.i18n('geminiModelAuto');
      case ModelMode.kGeminiPro:
        return this.i18n('geminiModelThinking');
      default:
        return '';
    }
  }

  protected getToolHeader_(): string {
    if (this.inputState && this.inputState.toolsSectionConfig) {
      return this.inputState.toolsSectionConfig.header;
    }
    return '';
  }

  protected getModelHeader_(): string {
    if (this.inputState && this.inputState.modelSectionConfig) {
      return this.inputState.modelSectionConfig.header;
    }
    return '';
  }

  protected getInputTypeLabel_(inputType: InputType): string {
    if (this.inputState && this.inputState.inputTypeConfigs) {
      const config =
          this.inputState.inputTypeConfigs.find(c => c.inputType === inputType);
      if (config && config.menuLabel) {
        return config.menuLabel;
      }
    }
    switch (inputType) {
      case InputType.kBrowserTab:
        return this.i18n('addTab');
      case InputType.kLensImage:
        return this.i18n('addImage');
      case InputType.kLensFile:
        return this.i18n('uploadFile');
      case InputType.kDrive:
        return this.i18n('addDriveFile');
      default:
        return '';
    }
  }

  // Checks if the drive upload item in the context menu should be visible.
  protected isDriveUploadAllowed_(): boolean {
    if (this.inputState) {
      return this.inputState.allowedInputTypes.includes(InputType.kDrive);
    }
    return false;
  }

  // Checks if the drive upload item in the context menu should be disabled.
  protected isDriveUploadDisabled_(): boolean {
    if (this.inputState) {
      return this.inputState.disabledInputTypes.includes(InputType.kDrive);
    }
    return this.fileNum >= this.maxFileCount_;
  }

  // Checks if the image upload item in the context menu should be visible.
  protected isImageUploadAllowed_(): boolean {
    if (this.inputState) {
      return this.inputState.allowedInputTypes.includes(InputType.kLensImage);
    }
    return false;
  }

  // Checks if the image upload item in the context menu should be disabled.
  protected isImageUploadDisabled_(): boolean {
    if (this.inputState) {
      return this.inputState.disabledInputTypes.includes(InputType.kLensImage);
    }
    return this.fileNum >= this.maxFileCount_;
  }

  // Checks if the file upload item in the context menu should be visible.
  protected isFileUploadAllowed_(): boolean {
    if (this.inputState) {
      return this.inputState.allowedInputTypes.includes(InputType.kLensFile);
    }
    return false;
  }

  // Checks if the file upload item in the context menu should be disabled.
  protected isFileUploadDisabled_(): boolean {
    if (this.inputState) {
      return this.inputState.disabledInputTypes.includes(InputType.kLensFile);
    }
    return this.fileNum >= this.maxFileCount_;
  }

  // Checks if the browser tab item in the context menu should be visible.
  protected isBrowserTabAllowed_(): boolean {
    if (this.inputState) {
      return this.inputState.allowedInputTypes.includes(InputType.kBrowserTab);
    }
    return false;
  }

  protected isMultiTabSelectionEnabledForShareTabsMode_(): boolean {
    return this.contextManagementInComposeboxEnabled_ && this.enableMultiTabSelection_;
  }

  // Checks if a tab item in the context menu should be disabled.
  protected isTabDisabled_(tab: TabInfo): boolean {
    let noNewContextAllowed = this.fileNum >= this.maxFileCount_;
    if (this.inputState) {
      noNewContextAllowed =
          this.inputState.disabledInputTypes.includes(InputType.kBrowserTab);
    }
    const isTabInContext = this.disabledTabIds.has(tab.tabId);
    // If multi-tab selection is enabled, we only want to disable a tab if
    // no more context can be added and the tab has not yet been added as
    // context already. Otherwise, don't disable the tab, since we want to allow
    // users to unselect the tab, and remove it from the context.
    if (this.isMultiTabSelectionEnabledForShareTabsMode_()) {
      return noNewContextAllowed && !isTabInContext;
    }
    return noNewContextAllowed || isTabInContext;
  }

  protected onSmartTabSharingToggleChange_(e: Event) {
    const toggle = e.target as CrToggleElement;
    this.fire('smart-tab-sharing-active-changed', {active: toggle.checked});
  }

  protected onSmartTabSharingItemClick_(e: Event) {
    if ((e.target as HTMLElement).id === 'smartTabSharingToggle') {
      return;
    }
    this.toggleSmartTabSharing_();
  }

  private toggleSmartTabSharing_() {
    const toggle = this.shadowRoot.querySelector<CrToggleElement>(
        '#smartTabSharingToggle')!;
    this.setSmartTabSharingToggle(!toggle.checked);
  }

  setSmartTabSharingToggle(active: boolean) {
    const toggle = this.shadowRoot.querySelector<CrToggleElement>(
        '#smartTabSharingToggle')!;
    toggle.checked = active;
    this.fire('smart-tab-sharing-active-changed', {active: toggle.checked});
  }

  protected onTabClick_(e: Event) {
    e.stopPropagation();

    const tabElement = e.currentTarget! as HTMLButtonElement;
    const tabInfo = this.tabSuggestions[Number(tabElement.dataset['index'])];

    assert(tabInfo);

    if (this.isMultiTabSelectionEnabledForShareTabsMode_() &&
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
  }

  protected addTabContext_(tabInfo: TabInfo) {
    this.fire('add-tab-context', {
      id: tabInfo.tabId,
      title: tabInfo.title,
      url: tabInfo.url,
      delayUpload: false,
      origin: TabUploadOrigin.CONTEXT_MENU,
    });
    if (!this.isMultiTabSelectionEnabledForShareTabsMode_()) {
      this.$.menu.close();
    }
  }

  protected onShareTabsRowPointerenter_() {
    this.pointerOverTrigger_ = true;
    this.cancelCloseTimer_();
    this.shareTabsFlyoutOpen_ = true;
  }

  protected onShareTabsRowPointerleave_() {
    this.pointerOverTrigger_ = false;
    this.scheduleCloseTimer_();
  }

  protected onShareTabsFlyoutPointerenter_() {
    this.pointerOverFlyout_ = true;
    this.cancelCloseTimer_();
  }

  protected onShareTabsFlyoutPointerleave_() {
    this.pointerOverFlyout_ = false;
    this.scheduleCloseTimer_();
  }

  private scheduleCloseTimer_() {
    this.cancelCloseTimer_();
    this.closeTimer_ = WindowProxy.getInstance().setTimeout(() => {
      this.closeTimer_ = null;
      if (!this.pointerOverTrigger_ && !this.pointerOverFlyout_) {
        this.shareTabsFlyoutOpen_ = false;
      }
    }, SHARE_TABS_FLYOUT_CLOSE_DELAY_MS);
  }

  private cancelCloseTimer_() {
    if (this.closeTimer_ !== null) {
      WindowProxy.getInstance().clearTimeout(this.closeTimer_);
      this.closeTimer_ = null;
    }
  }

  private resetShareTabsFlyout_() {
    this.cancelCloseTimer_();
    this.pointerOverTrigger_ = false;
    this.pointerOverFlyout_ = false;
    this.shareTabsFlyoutOpen_ = false;
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

  protected onImageUploadClick_() {
    this.fire('open-image-upload');
    this.$.menu.close();
  }

  protected onDriveUploadClick_() {
    this.fire('open-drive-upload');
    this.$.menu.close();
  }

  protected onFileUploadClick_() {
    this.fire('open-file-upload');
    this.$.menu.close();
  }

  protected onToolClick_(e: Event) {
    const toolMode = Number((e.currentTarget as HTMLElement).dataset['mode']);
    this.fire('tool-click', {toolMode});
    this.$.menu.close();
  }

  protected onModelClick_(e: Event) {
    const button = e.currentTarget as HTMLElement;
    const model = Number(button.dataset['model']) as ModelMode;
    this.fire('model-click', {model});
    this.$.menu.close();
  }

  protected onMenuClose_() {
    window.removeEventListener('blur', this.onWindowBlur_);
    this.resetShareTabsFlyout_();
    this.fire('close');
  }

  protected getIconForToolMode_(mode: ToolMode): string|undefined {
    return this.supportedTools_.get(mode)?.icon;
  }

  protected getIconForModelMode_(mode: ModelMode): string|undefined {
    return this.supportedModels_.get(mode)?.icon;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-contextual-action-menu': ContextualActionMenuElement;
  }
}

customElements.define(
    ContextualActionMenuElement.is, ContextualActionMenuElement);

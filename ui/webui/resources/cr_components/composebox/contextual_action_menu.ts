// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import './composebox_favicon_group.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
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

const SHARE_TABS_MENU_WIDTH_PX = 240;
const SHARE_TABS_FLYOUT_CLOSE_DELAY_MS = 300;
const SHARE_TABS_FLYOUT_GAP_PX = 4;

const ALIGNMENT_THRESHOLD_PX = 160;
const VIEWPORT_BUFFER_PX = 16;
const MIN_MENU_HEIGHT_PX = 100;

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
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fileNum: {type: Number},
      disabledTabIds: {type: Object},
      aimThreadRestoredTabs: {type: Array},
      tabSuggestions: {type: Array},
      inputState: {type: Object},
      smartTabSharingActive: {type: Boolean},
      smartTabSharingVisible: {type: Boolean},
      enableMultiTabSelection_: {
        reflect: true,
        type: Boolean,
      },
      tabPreviewUrl_: {type: String},
      tabPreviewsEnabled_: {type: Boolean},
      showContextMenuHeaders_: {type: Boolean},
      disableAutoReposition: {type: Boolean},
      contextManagementInComposeboxEnabled_: {
        reflect: true,
        type: Boolean,
        attribute: 'context-management-enabled',
      },
      shareTabsFlyoutOpen_: {type: Boolean},
      shareTabsFlyoutPosition_: {type: String},
      sharingTabsText_: {type: String},
      uploadButtonDisabled: {type: Boolean},
      isSidePanel: {type: Boolean},
      recentTabId: {type: Number},
    };
  }

  accessor recentTabId: number|null = null;
  accessor fileNum: number = 0;
  accessor disabledTabIds: Map<number, UnguessableToken> = new Map();
  accessor aimThreadRestoredTabs: TabInfo[] = [];
  accessor tabSuggestions: TabInfo[] = [];
  accessor inputState: InputState|null = null;
  accessor smartTabSharingActive: boolean = false;
  accessor smartTabSharingVisible: boolean = false;
  accessor disableAutoReposition: boolean = false;
  accessor uploadButtonDisabled: boolean = false;
  accessor isSidePanel: boolean = false;

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
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      getLoadTimeBoolean('contextManagementInComposeboxEnabled', false);
  protected accessor shareTabsFlyoutOpen_: boolean = false;
  protected accessor shareTabsFlyoutPosition_: string = 'right';
  protected accessor sharingTabsText_: string = '';

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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (this.contextManagementInComposeboxEnabled_) {
      if (changedProperties.has('disabledTabIds') ||
          changedProperties.has('aimThreadRestoredTabs')) {
        this.updateSharingTabsText_();
      }
      this.manageShareTabsInitialFocus_(changedProperties);
    }

    if (changedProperties.has('tabSuggestions') ||
        changedProperties.has('inputState')) {
      this.updateScrollable_();
    }
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
    // Show the menu initially to render it and measure its natural height.
    this.$.menu.showAt(anchor, {
      width: this.contextManagementInComposeboxEnabled_ ?
          SHARE_TABS_MENU_WIDTH_PX :
          MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });

    const rect = anchor.getBoundingClientRect();
    const menuHeight = this.$.menu.getDialog().offsetHeight;
    const spaceBelow = window.innerHeight - rect.bottom;
    const spaceAbove = rect.top;

    // Decide alignment strictly based on the threshold.
    const anchorAlignmentY = spaceBelow >= ALIGNMENT_THRESHOLD_PX ?
        AnchorAlignment.AFTER_END :
        AnchorAlignment.BEFORE_START;

    // Calculate max height based on the chosen alignment.
    const availableSpace = anchorAlignmentY === AnchorAlignment.AFTER_END ?
        spaceBelow :
        spaceAbove;
    const maxHeight = availableSpace - VIEWPORT_BUFFER_PX;

    // Constrain height if the menu is taller than available space.
    if (menuHeight > maxHeight) {
      const constrainedHeight = Math.max(MIN_MENU_HEIGHT_PX, maxHeight);
      this.$.menu.style.setProperty('--contextual-menu-max-height', `${constrainedHeight}px`);
    } else {
      this.$.menu.style.removeProperty('--contextual-menu-max-height');
    }

    // Position the menu using the finalized alignment.
    this.$.menu.showAt(anchor, {
      width: this.contextManagementInComposeboxEnabled_ ?
          SHARE_TABS_MENU_WIDTH_PX :
          MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: anchorAlignmentY,
      noOffset: true,
    });
    window.addEventListener('blur', this.onWindowBlur_);

    if (this.contextManagementInComposeboxEnabled_) {
      this.updateSharingTabsText_();
    }
    this.updateScrollable_();
  }

  private manageShareTabsInitialFocus_(
      changedProperties: PropertyValues<this>) {
    // Manually manage the initial keyboard focus for the "Share Tabs" menu item.
    // Because `tabSuggestions` are fetched asynchronously, the
    // `#shareTabsTrigger` button may not exist in the DOM at the exact moment
    // the menu is opened. This causes the underlying <cr-action-menu> to
    // incorrectly assign the initial focus to the next available item. To fix
    // this, we reclaim the focus once the tab data arrives and the DOM is updated.
    if (changedProperties.has('tabSuggestions')) {
      const isNowPopulated =
          this.tabSuggestions && this.tabSuggestions.length > 0;

      if (isNowPopulated && this.open) {
        requestAnimationFrame(() => {
          const triggerBtn =
              this.shadowRoot.querySelector<HTMLElement>('#shareTabsTrigger');
          if (triggerBtn) {
            triggerBtn.focus();
          }
        });
      }
    }
  }

  private updateSharingTabsText_() {
    const restoredCount = (this.aimThreadRestoredTabs?.length > 0) ?
        this.aimThreadRestoredTabs.length :
        0;
    const totalTabs = this.disabledTabIds.size + restoredCount;
    if (!this.contextManagementInComposeboxEnabled_ || totalTabs === 0) {
      this.sharingTabsText_ = this.i18n('shareTabs');
      return;
    }

    PluralStringProxyImpl.getInstance()
        .getPluralString('sharingTabs', totalTabs)
        .then((s: string) => {
          this.sharingTabsText_ = s;
        });
  }

  private isItemDisabled_<T>(item: T, disabledItems: T[]|undefined): boolean {
    if (this.uploadButtonDisabled) {
      return true;
    }
    if (!this.inputState || !disabledItems) {
      return true;
    }
    return disabledItems.includes(item);
  }

  protected isToolAllowed_(tool: ToolMode): boolean {
    return this.isItemAllowed_(tool, this.inputState?.allowedTools);
  }

  protected isToolDisabled_(tool: ToolMode): boolean {
    if (this.uploadButtonDisabled) {
      return true;
    }
    if (this.isToolActive_(tool)) {
      return false;
    }
    return this.isItemDisabled_(tool, this.inputState?.disabledTools);
  }

  protected isToolActive_(tool: ToolMode): boolean {
    if (!this.inputState) {
      return false;
    }
    return this.inputState.activeTool === tool;
  }

  protected isModelAllowed_(model: ModelMode): boolean {
    return this.isItemAllowed_(model, this.inputState?.allowedModels);
  }

  protected isModelDisabled_(model: ModelMode): boolean {
    return this.isItemDisabled_(model, this.inputState?.disabledModels);
  }

  protected isModelActive_(model: ModelMode): boolean {
    if (!this.inputState) {
      return false;
    }
    return this.inputState.activeModel === model;
  }

  protected isTabSelected_(tabOrId: TabInfo|number): boolean {
    if (typeof tabOrId === 'number') {
      return this.disabledTabIds.has(tabOrId);
    }
    const tab = tabOrId;
    const isAimThreadRestored =
        (this.aimThreadRestoredTabs || []).includes(tab);
    return this.disabledTabIds.has(tab.tabId) || isAimThreadRestored;
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

  private isItemAllowed_<T>(item: T, allowedItems: T[]|undefined): boolean {
    if (!this.inputState || !allowedItems) {
      return false;
    }
    return allowedItems.includes(item);
  }

  protected isInputTypeAllowed_(...types: InputType[]): boolean {
    return types.some(
        type => this.isItemAllowed_(type, this.inputState?.allowedInputTypes));
  }

  protected isInputTypeDisabled_(inputType: InputType): boolean {
    if (this.uploadButtonDisabled) {
      return true;
    }
    const limitReached = this.fileNum >= this.maxFileCount_;
    if (this.inputState) {
      return limitReached ||
          (this.inputState.disabledInputTypes || []).includes(inputType);
    }
    return limitReached;
  }

  // Checks if a tab item in the context menu should be disabled.
  protected isTabDisabled_(tab: TabInfo): boolean {
    const noNewContextAllowed =
        this.isInputTypeDisabled_(InputType.kBrowserTab);
    const isTabInContext = this.isTabSelected_(tab.tabId);
    if ((this.aimThreadRestoredTabs || []).includes(tab)) {
      return true;
    }
    if (this.enableMultiTabSelection_) {
      return noNewContextAllowed && !isTabInContext;
    }
    return noNewContextAllowed || isTabInContext;
  }

  protected getSelectedTabs_(): TabInfo[] {
    // Get the selected tab IDs from the `disabledTabIds` map and
    // `aimThreadRestoredTabs`. Because of how maps work in JS, the order when
    // converting to an array is least recently added to most recently added.
    const suggestionsMap =
        new Map(this.tabSuggestions.map(tab => [tab.tabId, tab]));
    const allSelectedIds = [
      ...this.disabledTabIds.keys(),
    ];

    // Get selected tabs in the order they were added. But because the selected
    // IDs lists have tabIds, and not the TabInfo, convert them back to an array
    // of non-empty TabInfos. Then, reverse it to get most recent item first so
    // its favicon is always leftmost.
    const activeRestoredTabs = allSelectedIds.map(id => suggestionsMap.get(id))
                                   .filter((tab): tab is TabInfo => !!tab)
                                   .reverse();

    return activeRestoredTabs.concat(this.aimThreadRestoredTabs || []);
  }

  protected isRecentTab_(tabId: number): boolean {
    return this.recentTabId !== null && tabId === this.recentTabId;
  }

  protected async onSmartTabSharingItemClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const isFlyout = target.id === 'smartTabSharingItemFlyout';
    this.toggleSmartTabSharing_();
    if (isFlyout) {
      this.$.menu.close();
    } else {
      await this.updateComplete;
      const trigger =
          this.shadowRoot.querySelector<HTMLElement>('#shareTabsTrigger');
      if (trigger) {
        trigger.focus();
      }
    }
  }

  private toggleSmartTabSharing_() {
    this.fire('smart-tab-sharing-active-changed', {
      active: !this.smartTabSharingActive,
    });
  }

  setSmartTabSharingToggle(active: boolean) {
    this.fire('smart-tab-sharing-active-changed', {active});
  }

  protected onTabClick_(e: Event) {
    e.stopPropagation();

    const tabElement = e.currentTarget! as HTMLButtonElement;
    const tabInfo = this.tabSuggestions[Number(tabElement.dataset['index'])];

    assert(tabInfo);

    if (this.enableMultiTabSelection_ && this.isTabSelected_(tabInfo.tabId)) {
      this.deleteTabContext_(this.disabledTabIds.get(tabInfo.tabId)!);
      return;
    }
    this.addTabContext_(tabInfo);
    recordContextAdditionMethod(
        ComposeboxContextAddedMethod.CONTEXT_MENU, this.metricsSource_);
  }

  protected deleteTabContext_(uuid: UnguessableToken) {
    this.fire('delete-tab-context', {uuid: uuid, fromUserAction: true});
    if (!this.enableMultiTabSelection_ ||
        this.metricsSource_ === 'NewTabPage' ||
        this.metricsSource_ === 'Omnibox') {
      this.$.menu.close();
    }
  }

  protected addTabContext_(tabInfo: TabInfo) {
    this.fire('add-tab-context', {
      id: tabInfo.tabId,
      title: tabInfo.title,
      url: tabInfo.url,
      delayUpload: false,
      origin: TabUploadOrigin.CONTEXT_MENU,
    });
    if (!this.enableMultiTabSelection_ ||
        this.metricsSource_ === 'NewTabPage' ||
        this.metricsSource_ === 'Omnibox') {
      this.$.menu.close();
    }
  }

  private get hasTabSuggestions_(): boolean {
    return !!this.tabSuggestions && this.tabSuggestions.length > 0;
  }

  protected onShareTabsRowPointerenter_() {
    if (!this.hasTabSuggestions_) {
      return;
    }
    this.pointerOverTrigger_ = true;
    this.cancelCloseTimer_();
    this.shareTabsFlyoutOpen_ = true;
    this.updateFlyoutPosition_();
  }

  protected onShareTabsRowPointerleave_() {
    if (!this.hasTabSuggestions_) {
      return;
    }
    this.pointerOverTrigger_ = false;
    this.scheduleCloseTimer_();
  }

  protected onShareTabsFlyoutPointerenter_() {
    if (!this.hasTabSuggestions_) {
      return;
    }
    this.pointerOverFlyout_ = true;
    this.cancelCloseTimer_();
  }

  protected onShareTabsFlyoutPointerleave_() {
    if (!this.hasTabSuggestions_) {
      return;
    }
    this.pointerOverFlyout_ = false;
    this.scheduleCloseTimer_();
  }

  protected onShareTabsRowKeydown_(e: KeyboardEvent) {
    if (e.key === 'ArrowRight' || e.key === 'Enter' || e.key === ' ') {
      if (!this.hasTabSuggestions_) {
        return;
      }
      e.preventDefault();
      e.stopPropagation();
      this.shareTabsFlyoutOpen_ = true;
      this.updateFlyoutPosition_();

      this.updateComplete.then(() => {
        const firstTabItem = this.shadowRoot.querySelector<HTMLElement>(
            '.share-tabs-flyout button.dropdown-item:not([disabled])');
        if (firstTabItem) {
          firstTabItem.focus();
        }
      });
    }
  }

  protected onShareTabsFlyoutKeydown_(e: KeyboardEvent) {
    if (e.key === 'ArrowLeft' || e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      this.shareTabsFlyoutOpen_ = false;

      const row =
          this.shadowRoot.querySelector<HTMLElement>('#shareTabsTrigger');
      if (row) {
        row.focus();
      }
    }
  }

  private updateFlyoutPosition_() {
    this.updateComplete.then(() => {
      const trigger =
          this.shadowRoot.querySelector<HTMLElement>('#shareTabsTrigger');
      const flyout =
          this.shadowRoot.querySelector<HTMLElement>('.share-tabs-flyout');
      if (!trigger || !flyout) {
        return;
      }

      const triggerRect = trigger.getBoundingClientRect();
      const flyoutWidth = flyout.offsetWidth || 320;
      const viewportWidth = window.innerWidth;

      flyout.style.top = `${triggerRect.top}px`;

      if (triggerRect.right + flyoutWidth + SHARE_TABS_FLYOUT_GAP_PX <=
          viewportWidth) {
        this.shareTabsFlyoutPosition_ = 'right';
        flyout.style.left = `${triggerRect.right + SHARE_TABS_FLYOUT_GAP_PX}px`;
      } else if (triggerRect.left >= flyoutWidth + SHARE_TABS_FLYOUT_GAP_PX) {
        this.shareTabsFlyoutPosition_ = 'left';
        flyout.style.left =
            `${triggerRect.left - flyoutWidth - SHARE_TABS_FLYOUT_GAP_PX}px`;
      } else {
        this.shareTabsFlyoutPosition_ = 'bottom';
        flyout.style.top = `${triggerRect.bottom + SHARE_TABS_FLYOUT_GAP_PX}px`;
        const rtl = getComputedStyle(this).direction === 'rtl';
        if (rtl) {
          flyout.style.left = `${triggerRect.right - flyoutWidth}px`;
        } else {
          flyout.style.left = `${triggerRect.left}px`;
        }
      }
    });
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

  private updateScrollable_() {
    this.updateComplete.then(() => {
      const dialog = this.$.menu.getDialog();
      const isScrollable = dialog.scrollHeight > dialog.offsetHeight;
      this.$.menu.toggleAttribute('scrollable', isScrollable);
    });
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
    this.$.menu.style.removeProperty('--contextual-menu-max-height');
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

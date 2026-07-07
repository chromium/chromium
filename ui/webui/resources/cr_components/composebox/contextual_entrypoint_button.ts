// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import './composebox_favicon_group.js';
import './contextual_action_menu.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getLoadTimeBoolean, GlifAnimationState, recordBoolean, recordUserAction} from './common.js';
import {getCss} from './contextual_entrypoint_button.css.js';
import {getHtml} from './contextual_entrypoint_button.html.js';
import {WindowProxy} from './window_proxy.js';

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
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      showContextMenuDescription: {type: Boolean},
      showSuggestionLabel: {type: Boolean, reflect: true},
      inputState: {type: Object},
      glifAnimationState: {type: String, reflect: true},
      uploadButtonDisabled: {type: Boolean},
      hasPopupFocus: {type: Boolean, reflect: true},
      applyContextButtonBackground: {type: Boolean, reflect: true},
      isOblongShape: {type: Boolean, reflect: true},
      windowWidthBelowThreshold_: {type: Boolean},
      sharedTabs: {type: Array},
      restoredTabs: {type: Array},
      tabFaviconChipsToCoinsEnabled_: {type: Boolean},
      energyEffectAnimationEnabled: {type: Boolean, reflect: true},
      disableFallbackGlifAnimation: {type: Boolean},
      smartTabSharingActive: {type: Boolean},
      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor showContextMenuDescription: boolean = false;
  accessor showSuggestionLabel: boolean = false;
  accessor inputState: InputState|null = null;
  accessor sharedTabs: TabInfo[] = [];
  accessor restoredTabs: TabInfo[] = [];
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor uploadButtonDisabled: boolean = false;
  accessor hasPopupFocus: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  accessor isOblongShape: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor disableFallbackGlifAnimation: boolean = false;
  accessor smartTabSharingActive: boolean = false;
  protected accessor windowWidthBelowThreshold_: boolean = false;
  protected accessor isLensSearchbox_: boolean =
      loadTimeData.valueExists('isLensSearchbox') &&
      loadTimeData.getBoolean('isLensSearchbox');
  protected accessor tabFaviconChipsToCoinsEnabled_: boolean =
      loadTimeData.getBoolean('tabFaviconChipsToCoinsEnabled');
  private showContextMenuDescriptionEnabled_: boolean =
      getLoadTimeBoolean('composeboxShowContextMenuDescription', false);
  private metricsSource_: string = loadTimeData.getString('composeboxSource');
  private eventTracker_: EventTracker = new EventTracker();
  private hasRecordedShown_: boolean = false;
  private hasRecordedHover_: boolean = false;

  constructor() {
    super();
  }

  // Return reversed lists of restored (historical) and
  // shared tabs (selected tabs).
  protected getTabs_(): TabInfo[] {
    if (this.smartTabSharingActive) {
      return [];
    }
    const reversedShared = [...this.sharedTabs].reverse();
    const reversedRestored = [...(this.restoredTabs || [])].reverse();
    return reversedShared.concat(reversedRestored);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        WindowProxy.getInstance().matchMedia('(width <= 264px)'), 'change',
        (e: MediaQueryListEvent) => {
          this.windowWidthBelowThreshold_ = e.matches;
        });
    if (!this.hasRecordedShown_) {
      recordUserAction(
          'ContextualSearch.AddTabsButton.Shown.' + this.metricsSource_);
      this.hasRecordedShown_ = true;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('inputState') && this.inputState) {
      const inToolMode = this.inputState.activeTool !== ToolMode.kUnspecified;

      if (this.showContextMenuDescriptionEnabled_) {
        this.showContextMenuDescription = !inToolMode;
      }
    }
  }

  protected onEntrypointClick_(e: Event) {
    e.stopPropagation();

    recordUserAction(
        'ContextualSearch.AddTabsButton.Clicked.' + this.metricsSource_);

    const metricName =
        'ContextualSearch.ContextMenuEntry.Clicked.' + this.metricsSource_;
    recordBoolean(metricName, true);
    const entrypoint =
        this.shadowRoot.querySelector<HTMLElement>('#entrypoint');
    assert(entrypoint);
    this.fire('context-menu-entrypoint-click', {
      x: entrypoint.getBoundingClientRect().left,
      y: entrypoint.getBoundingClientRect().bottom,
    });
  }

  protected onEntrypointPointerenter_() {
    if (!this.hasRecordedHover_) {
      recordUserAction(
          'ContextualSearch.AddTabsButton.Hovered.' + this.metricsSource_);
      this.hasRecordedHover_ = true;
    }
    this.fire('context-menu-entrypoint-hover');
  }

  protected onIconAnimationend_(e: AnimationEvent) {
    this.onAnimationEnd_(e, 'icon-rotate');
  }

  protected onDescriptionAnimationend_(e: AnimationEvent) {
    this.onAnimationEnd_(e, 'slide-in');
  }

  protected onAimBackgroundAnimationend_(e: AnimationEvent) {
    if (this.showContextMenuDescription) {
      return;
    }

    this.onAnimationEnd_(e, 'background-fade');
  }

  private onAnimationEnd_(e: AnimationEvent, animationName: string) {
    if (e.animationName === animationName) {
      this.glifAnimationState = GlifAnimationState.FINISHED;
    }
  }

  protected getWrapperId_(): string {
    return this.glifAnimationState !== GlifAnimationState.INELIGIBLE ?
        'glowWrapper' :
        '';
  }

  protected getWrapperCssClass_(): string {
    return this.glifAnimationState !== GlifAnimationState.INELIGIBLE ?
        'glow-container' :
        '';
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

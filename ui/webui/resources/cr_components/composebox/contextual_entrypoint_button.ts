// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import './contextual_action_menu.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {GlifAnimationState, recordBoolean} from './common.js';
import {getCss} from './contextual_entrypoint_button.css.js';
import {getHtml} from './contextual_entrypoint_button.html.js';

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
    return getHtml.bind(this as any)();
  }

  static override get properties() {
    return {
      // =========================================================================
      // Public properties
      // =========================================================================
      showContextMenuDescription: {type: Boolean},
      inputState: {type: Object},
      glifAnimationState: {type: String, reflect: true},
      uploadButtonDisabled: {type: Boolean},
      hasPopupFocus: {type: Boolean, reflect: true},
      windowWidthBelowThreshold_: {type: Boolean},
    };
  }

  accessor showContextMenuDescription: boolean = false;
  accessor inputState: InputState|null = null;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor uploadButtonDisabled: boolean = false;
  accessor hasPopupFocus: boolean = false;
  protected accessor windowWidthBelowThreshold_: boolean = false;

  private metricsSource_: string = loadTimeData.getString('composeboxSource');
  private usePecApi_: boolean =
      loadTimeData.valueExists('contextualMenuUsePecApi') ?
      loadTimeData.getBoolean('contextualMenuUsePecApi') :
      false;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        window.matchMedia('(width <= 264px)'), 'change',
        (e: MediaQueryListEvent) => {
          this.windowWidthBelowThreshold_ = e.matches;
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected onEntrypointClick_(e: Event) {
    e.stopPropagation();

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

  protected onDescriptionAnimationEnd_(e: AnimationEvent) {
    this.onAnimationEnd_(e, 'slide-in');
  }

  protected onAimBackgroundAnimationEnd_(e: AnimationEvent) {
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

  protected hasAllowedInputs_(): boolean {
    if (!this.usePecApi_) {
      return true;
    }
    return !!this.inputState &&
        (this.inputState.allowedModels.length > 0 ||
         this.inputState.allowedTools.length > 0 ||
         this.inputState.allowedInputTypes.length > 0);
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

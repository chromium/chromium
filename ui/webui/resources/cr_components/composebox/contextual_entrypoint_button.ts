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
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getLoadTimeBoolean, GlifAnimationState, recordBoolean} from './common.js';
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
    return getHtml.bind(this as any)();
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
      windowWidthBelowThreshold_: {type: Boolean},
      isOblongShape_: {type: Boolean, reflect: true},
    };
  }

  accessor showContextMenuDescription: boolean = false;
  accessor showSuggestionLabel: boolean = false;
  accessor inputState: InputState|null = null;
  accessor glifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor uploadButtonDisabled: boolean = false;
  accessor hasPopupFocus: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  protected accessor windowWidthBelowThreshold_: boolean = false;
  protected accessor isOblongShape_: boolean =
      getLoadTimeBoolean('contextButtonShapeIsOblong', false);

  private contextButtonHasBackground_: boolean =
      getLoadTimeBoolean('contextButtonHasBackground', false);
  private showContextMenuDescriptionEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenuDescription');
  private metricsSource_: string = loadTimeData.getString('composeboxSource');
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        WindowProxy.getInstance().matchMedia('(width <= 264px)'), 'change',
        (e: MediaQueryListEvent) => {
          this.windowWidthBelowThreshold_ = e.matches;
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('inputState') && this.inputState) {
      const inToolMode = this.inputState.activeTool !== ToolMode.kUnspecified;

      this.applyContextButtonBackground =
          this.contextButtonHasBackground_ && !inToolMode;

      if (this.showContextMenuDescriptionEnabled_) {
        this.showContextMenuDescription = !inToolMode;
      }
    }
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

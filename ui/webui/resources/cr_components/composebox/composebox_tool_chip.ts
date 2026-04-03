// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolMode} from './composebox_query.mojom-webui.js';
import type {InputState} from './composebox_query.mojom-webui.js';
import {getCss} from './composebox_tool_chip.css.js';
import {getHtml} from './composebox_tool_chip.html.js';

export class ComposeboxToolChipElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'cr-composebox-tool-chip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.call(this);
  }

  static override get properties() {
    return {
      inputState: {type: Object},
      isCanvasQuerySubmitted: {type: Boolean},
    };
  }

  protected accessor inputState: InputState|null = null;
  accessor isCanvasQuerySubmitted: boolean = false;


  protected getToolChipLabel_(): string {
    if (!this.inputState) {
      return '';
    }

    if (this.inputState.toolConfigs) {
      const config = this.inputState.toolConfigs.find(
          c => c.tool === this.inputState!.activeTool);
      if (config && config.chipLabel) {
        return config.chipLabel;
      }
    }
    // Fallback to i18n strings
    switch (this.inputState.activeTool) {
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

  protected get isCanvasActive_(): boolean {
    return this.inputState?.activeTool === ToolMode.kCanvas &&
        this.isCanvasQuerySubmitted;
  }

  protected getIcon_(): string {
    if (!this.inputState) {
      return '';
    }

    switch (this.inputState.activeTool) {
      case ToolMode.kDeepSearch:
        return 'composebox:deepSearch';
      case ToolMode.kImageGen:
        return 'composebox:nanoBanana';
      case ToolMode.kCanvas:
        return 'composebox:canvas';
      default:
        return '';
    }
  }

  protected onClick_() {
    if (this.inputState) {
      // Client-side workaround: The Canvas chip should be non-removable once
      // active, as per launch requirements (see crbug.com/491479366). A future
      // proper fix might involve server-side config or handling
      // TOOL_SUBSTATE_CANVAS_FOLLOWUP (see crbug.com/491591419).
      if (this.isCanvasActive_) {
        return;
      }
      this.fire('tool-click', {toolMode: this.inputState.activeTool});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-tool-chip': ComposeboxToolChipElement;
  }
}

customElements.define(ComposeboxToolChipElement.is, ComposeboxToolChipElement);

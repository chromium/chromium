// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';
import './searchbox_config_icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getLoadTimeBoolean} from './common.js';
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

  protected getIcon_(): string {
    if (!this.inputState) {
      return '';
    }

    if (getLoadTimeBoolean('useSearchboxConfigIconIds', false)) {
      const config = this.inputState.toolConfigs?.find(
          c => c.tool === this.inputState!.activeTool);
      return `searchbox_config:${config ? config.icon : 0}`;
    }

    switch (this.inputState.activeTool) {
      case ToolMode.kDeepSearch:
        return 'composebox:travel-explore';
      case ToolMode.kImageGen:
        return this.isClankMode_() ? 'composebox:nanoBanana-clank' :
                                     'composebox:nanoBanana-custom';
      case ToolMode.kCanvas:
        return 'composebox:draft-spark';
      default:
        return '';
    }
  }

  protected isClankMode_(): boolean {
    if (!getLoadTimeBoolean('isAndroid', false)) {
      return false;
    }
    const tool = this.inputState?.activeTool;
    return tool === ToolMode.kCanvas || tool === ToolMode.kImageGen;
  }

  protected getModeClasses_(): string {
    if (!this.isClankMode_()) {
      return '';
    }
    return this.inputState?.activeTool === ToolMode.kImageGen ?
        'clank image-gen' :
        'clank';
  }

  protected onClick_() {
    if (this.inputState) {
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

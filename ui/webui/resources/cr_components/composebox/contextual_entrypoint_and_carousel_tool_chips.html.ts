// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolMode as ComposeboxToolMode} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';

export function getHtml(this: ContextualEntrypointAndCarouselElement) {
  // clang-format off
  return html`
${this.showRecentTabChip ? html`
  <composebox-recent-tab-chip id="recentTabChip"
      class="upload-button contextual-chip"
      .recentTab="${this.recentTabForChip}"
      @add-tab-context="${this.addTabContext_}">
  </composebox-recent-tab-chip>
` : ''}
${this.shouldShowLensSearchChip_ ? html`
  <cr-composebox-lens-search id="lensSearchChip"
      class="upload-button contextual-chip">
  </cr-composebox-lens-search>
` : ''}
${this.activeTool_ === ComposeboxToolMode.kDeepSearch ? html`
  <cr-composebox-tool-chip
      id="deepSearchChip"
      exportparts="tool-chip-label"
      icon="composebox:deepSearch"
      label="${this.getToolChipLabel_(ComposeboxToolMode.kDeepSearch)}"
      remove-chip-aria-label="${
      this.i18n(
          'removeToolChipAriaLabel',
          this.getToolChipLabel_(ComposeboxToolMode.kDeepSearch))}"
      ?visible="${true}"
      @click="${this.handleDeepSearchClick_}">
  </cr-composebox-tool-chip>
` : ''}
${this.activeTool_ === ComposeboxToolMode.kImageGen ? html`
  <cr-composebox-tool-chip
      id="nanoBananaChip"
      exportparts="tool-chip-label"
      icon="composebox:nanoBanana"
      label="${this.getToolChipLabel_(ComposeboxToolMode.kImageGen)}"
      remove-chip-aria-label="${
      this.i18n(
          'removeToolChipAriaLabel',
          this.getToolChipLabel_(ComposeboxToolMode.kImageGen))}"
      ?visible="${true}"
      @click="${this.handleImageGenClick_}">
  </cr-composebox-tool-chip>
` : ''}
${this.activeTool_ === ComposeboxToolMode.kCanvas ? html`
  <cr-composebox-tool-chip
      id="canvasChip"
      exportparts="tool-chip-label"
      icon="composebox:canvas"
      label="${this.getToolChipLabel_(ComposeboxToolMode.kCanvas)}"
      remove-chip-aria-label="${
      this.i18n(
          'removeToolChipAriaLabel',
          this.getToolChipLabel_(ComposeboxToolMode.kCanvas))}"
      ?visible="${true}"
      @click="${this.handleCanvasClick_}">
  </cr-composebox-tool-chip>
` : ''}`;
  // clang-format on
}

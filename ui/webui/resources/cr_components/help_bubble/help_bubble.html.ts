// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {HelpBubbleElement} from './help_bubble.js';

export function getHtml(this: HelpBubbleElement) {
  return html`
<link rel="stylesheet" href="chrome://theme/colors.css?sets=ui,chrome&shadow_host=true">
<div class="help-bubble" role="alertdialog" aria-modal="true"
    aria-labelledby="title" aria-describedby="body" aria-live="assertive"
    @keydown="${this.onKeyDown_}" @click="${this.blockPropagation_}">
  <div id="topContainer">
    <div id="bodyIcon" ?hidden="${!this.shouldShowBodyIcon_()}"
        role="image" aria-label="${this.bodyIconAltText}">
      <cr-icon icon="iph:${this.bodyIconName}"></cr-icon>
    </div>
    <div id="progress" ?hidden="${!this.progress}" role="progressbar"
        aria-valuenow="${this.progress ? this.progress.current : nothing}"
        aria-valuemin="1"
        aria-valuemax="${this.progress ? this.progress.total : nothing}">
      ${this.progressData_.map((_item, index) => html`
        <div class="${this.getProgressClass_(index)}"></div>`)}
    </div>
    <h1 id="title"
        ?hidden="${!this.shouldShowTitleInTopContainer_()}">
      ${this.titleText}
    </h1>
    <p id="topBody"
        ?hidden="${!this.shouldShowBodyInTopContainer_()}">
      ${this.bodyText}
    </p>
    <cr-icon-button id="close" iron-icon="cr:close"
        aria-label="${this.closeButtonAltText}" @click="${this.dismiss_}"
        tabindex="${this.closeButtonTabIndex}">
    </cr-icon-button>
  </div>
  <div id="main" ?hidden="${!this.shouldShowBodyInMain_()}">
    <div id="middleRowSpacer" ?hidden="!${this.shouldShowBodyIcon_()}">
    </div>
    <p id="mainBody">${this.bodyText}</p>
  </div>
  <div id="buttons" ?hidden="${!this.buttons.length}">
    ${this.sortedButtons.map(item => html`
      <cr-button id="${this.getButtonId_(item)}"
          tabindex="${this.getButtonTabIndex_(item)}"
          class="${this.getButtonClass_(item.isDefault)}"
          @click="${this.onButtonClick_}"
          role="button" aria-label="${item.text}">${item.text}</cr-button>`)}
  </div>
  <div id="arrow" class="${this.getArrowClass_()}">
    <div id="inner-arrow"></div>
  </div>
</div>`;
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxIconElement} from './searchbox_icon.js';

export function getHtml(this: SearchboxIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container"
    style="--container-bg-color:${this.getContainerBgColor_()};">
  <img id="image" src="${this.imageSrc_}" ?hidden="${!this.showImage_}"
      @load="${this.onImageLoad_}" @error="${this.onImageError_}">

  <div ?hidden="${this.showIconImg_}">
    <div id="icon" style="-webkit-mask-image: ${this.maskImage};"
        ?hidden="${this.showFaviconImage_}">
    </div>
    <div id="faviconImageContainer"
        ?hidden="${!this.showFaviconImage_}">
      <img id="faviconImage" src="${this.faviconImage_}"
          srcset="${this.faviconImageSrcSet_}"
          @load="${this.onFaviconLoad_}"
          @error="${this.onFaviconError_}">
    </div>
  </div>

  <img id="iconImg" src="${this.iconSrc_}" ?hidden="${!this.showIconImg_}"
      @load="${this.onIconLoad_}">
</div>
<!--_html_template_end_-->`;
  // clang-format on
}

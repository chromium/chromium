// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrUrlListItemElement} from './cr_url_list_item.js';

function getImageHtml(this: CrUrlListItemElement, item: string, index: number) {
  if (!this.shouldShowImageUrl_(item, index)) {
    return '';
  }

  return html`
<div class="image-container" ?hidden="${!this.firstImageLoaded_}">
  <img class="folder-image" is="cr-auto-img" auto-src="${item}"
      draggable="false">
</div>`;
}

function getFolderImagesHtml(this: CrUrlListItemElement) {
  if (!this.shouldShowFolderImages_()) {
    return '';
  }
  return html`${
      this.imageUrls.map(
          (item, index) => getImageHtml.bind(this)(item, index))}`;
}

export function getHtml(this: CrUrlListItemElement) {
  return html`
<a id="anchor" .href="${this.url}" ?hidden="${!this.asAnchor}"
    target="${this.asAnchorTarget}"
    aria-label="${this.getItemAriaLabel_()}"
    aria-description="${this.getItemAriaDescription_() || nothing}">
</a>
<button id="button" ?hidden="${this.asAnchor}"
    aria-label="${this.getItemAriaLabel_()}"
    aria-description="${this.getItemAriaDescription_() || nothing}">
</button>

<div id="item">
  <slot name="prefix"></slot>
  <div id="iconContainer">
    <div class="favicon" ?hidden="${!this.shouldShowFavicon_()}"
        .style="background-image: ${this.getFavicon_()};">
    </div>
    <div class="image-container" ?hidden="${!this.shouldShowUrlImage_()}">
      <img class="url-image" is="cr-auto-img" auto-src="${this.imageUrls[0]}"
          draggable="false">
    </div>
    <div class="folder-and-count"
        ?hidden="${!this.shouldShowFolderCount_()}">
      ${getFolderImagesHtml.bind(this)()}
      <slot id="folder-icon" name="folder-icon">
        <div class="folder cr-icon icon-folder-open"
            ?hidden="${!this.shouldShowFolderIcon_()}"></div>
      </slot>
      <div class="count">${this.getDisplayedCount_()}</div>
    </div>
  </div>
  <slot id="content" name="content" @slotchange="${this.onContentSlotChange_}">
  </slot>
  <div id="metadata" class="metadata">
    <span class="title">${this.title}</span>
    <div class="descriptions">
      <div class="description" ?hidden="${!this.description}">
        <span class="description-text">${this.description}</span>
        <span class="description-meta" ?hidden="${!this.descriptionMeta}">
          &middot; ${this.descriptionMeta}
        </span>
      </div>
      <div id="badgesContainer" class="badges">
        <slot id="badges" name="badges"
            @slotchange="${this.onBadgesSlotChange_}">
        </slot>
      </div>
    </div>
  </div>
  <div class="suffix">
    <slot name="suffix"></slot>
  </div>
</div>
<slot name="footer"></slot>
`;
}

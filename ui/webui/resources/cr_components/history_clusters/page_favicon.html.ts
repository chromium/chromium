// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageFaviconElement} from './page_favicon.js';

export function getHtml(this: PageFaviconElement) {
  return this.imageUrl_ ? html`<img id="page-image"
      is="cr-auto-img" auto-src="${this.imageUrl_.url}"></img>` :
                          '';
}

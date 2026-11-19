// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  return html`<!--_html_template_start_-->
<div>
  <textarea id="short"></textarea>
  <textarea id="wrap-for-end-tag" class="thirty-eight-char-class-pushes-over">
  </textarea>
  <textarea id="multiline" autocomplete="off" spellcheck="false"
      placeholder="Enter text"
      @paste="${this.onInputPaste_}" @scroll="${this.onInputScroll_}">
  </textarea>
  <textarea id="previously-wrapped" rows="4"
      cols="60">
  </textarea>
  <textarea id="preset-multiline">
Line 1
  Indented line 2</textarea>
  <title id="page-title"
      class="medium-class-name-to-wrap-attrs">${this.pageTitle}</title>
  <svg viewBox="0 0 24 24">
    <title id="svg-title"
        class="svg-title-is-not-rcdata-and-wraps-across-lines">Icon</title>
  </svg>
</div>
<!--_html_template_end_-->`;
}

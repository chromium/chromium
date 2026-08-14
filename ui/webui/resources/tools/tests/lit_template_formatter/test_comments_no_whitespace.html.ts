// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="title-area" role="gridcell"><!--
  Can't have any line breaks.
--><a is="action-link" id="fileLink" href="${this.data?.url || ''}"></a><!--
  Before #name.
--><span id="name" title="${this.data?.fileName || ''}"></span>
</div>
<span><!-- Whitespace is preserved in this span. Ignore new lines.
  --><span>${this.beforeText}</span><!--
  --><mark aria-description="${this.highlightDescription}"><!--
    -->${this.highlightedText}<!--
  --></mark><!--
  --><span>${this.afterText}</span><!--
--></span>
<!--_html_template_end_-->`;
  // clang-format on
}

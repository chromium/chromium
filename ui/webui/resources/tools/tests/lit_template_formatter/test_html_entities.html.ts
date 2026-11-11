// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  return html`<!--_html_template_start_-->
<div>
  <!-- Comment with &amp; and &nbsp; -->
  <span>&nbsp;</span>
  <span>...&nbsp;</span>
  <div>A &amp; B &lt;cr-button&gt; Home > Settings &harr; &#9888; &#x200B;</div>
  <div title="Foo &quot;bar&quot; &amp; &nbsp;">${this.label}</div>
</div>
<!--_html_template_end_-->`;
}

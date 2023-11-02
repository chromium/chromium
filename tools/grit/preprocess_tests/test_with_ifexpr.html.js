// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getTemplate() {
  return `<!--_html_template_start_--><html>
  <head>
  <meta charset="utf-8">
  </head>
  <body>
  Always present text
  <if expr="foo">
  Text for foo only
  </if>
  <if expr="bar">
  Text for bar only
  </if>
  More text for everyone
  </body>
  </html><!--_html_template_end_-->`;
}

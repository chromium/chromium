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
  
  Text for foo only
  
  <!--grit-removed-lines:2-->
  More text for everyone
  </body>
  </html><!--_html_template_end_-->`;
}

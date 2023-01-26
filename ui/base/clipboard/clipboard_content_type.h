// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_CONTENT_TYPE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_CONTENT_TYPE_H_

namespace ui {

// This enum corresponds to the sanitization of the clipboard's payload. It is
// used throughout to add context to the payload, which is important to add
// Windows-only headers correctly before writing HTML to the clipboard.
// The following example shows the differences between a sanitized HTML and
// unsanitized HTML.
// Copied HTML
// <html>
//   <head> <style>p {color:blue}</style> </head>
//   <body>
//     <p>Hello World</p>
//     <script> alert("Hello World!"); </script>
//   </body>
// </html>
// Sanitized HTML: Inlined styles (shortened for brevity) and no <script>.
// <p style="color: blue; font-size: medium; font-style: normal;
// font-variant-ligatures: normal; font-variant-caps: normal; font-weight: 400;
// ... ">Hello World</p>
// Unsanitized HTML: <head> and <script> content included.
// <html>
//   <head> <style>p {color:blue}</style> </head>
//   <body>
//     <p>Hello World</p>
//     <script> alert("Hello World!"); </script>
//   </body>
// </html>
// See the following doc for more information on Windows-only headers:
// https://docs.google.com/document/d/1YSw4nlxV7nSA0PptO1oQtNo_Qn1IgIg5rgbGu7kQBak/edit?usp=sharing
enum class ClipboardContentType { kSanitized, kUnsanitized };

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_CONTENT_TYPE_H_

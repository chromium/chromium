/**
@license
Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at
http://polymer.github.io/LICENSE.txt The complete set of authors may be found at
http://polymer.github.io/AUTHORS.txt The complete set of contributors may be
found at http://polymer.github.io/CONTRIBUTORS.txt Code distributed by Google as
part of the polymer project is also subject to an additional IP rights grant
found at http://polymer.github.io/PATENTS.txt
*/
/*
Note that this file probably doesn't do what you expect it to do. It's not
a `<style is=custom-style include="..."` type of style include, which mean
these styles will only apply to the main document, regardless of where
you import this file.

For a set of styles that can be applied to an element, check
paper-styles/typography.html.
*/
import '../../font-roboto/roboto.js';
import {html} from '../../polymer/polymer_bundled.min.js';
const template = html`
<style>

.paper-font-display4,
.paper-font-display3,
.paper-font-display2,
.paper-font-display1,
.paper-font-headline,
.paper-font-title,
.paper-font-subhead,
.paper-font-body2,
.paper-font-body1,
.paper-font-caption,
.paper-font-menu,
.paper-font-button {
  font-family: 'Roboto', 'Noto', sans-serif;
  -webkit-font-smoothing: antialiased;  /* OS X subpixel AA bleed bug */
}

.paper-font-code2,
.paper-font-code1 {
  font-family: 'Roboto Mono', 'Consolas', 'Menlo', monospace;
  -webkit-font-smoothing: antialiased;  /* OS X subpixel AA bleed bug */
}

/* Opt for better kerning for headers &amp; other short labels. */
.paper-font-display4,
.paper-font-display3,
.paper-font-display2,
.paper-font-display1,
.paper-font-headline,
.paper-font-title,
.paper-font-subhead,
.paper-font-menu,
.paper-font-button {
  text-rendering: optimizeLegibility;
}

/*
"Line wrapping only applies to Body, Subhead, Headline, and the smaller Display
styles. All other styles should exist as single lines."
*/
.paper-font-display4,
.paper-font-display3,
.paper-font-title,
.paper-font-caption,
.paper-font-menu,
.paper-font-button {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.paper-font-display4 {
  font-size: 112px;
  font-weight: 300;
  letter-spacing: -.044em;
  line-height: 120px;
}

.paper-font-display3 {
  font-size: 56px;
  font-weight: 400;
  letter-spacing: -.026em;
  line-height: 60px;
}

.paper-font-display2 {
  font-size: 45px;
  font-weight: 400;
  letter-spacing: -.018em;
  line-height: 48px;
}

.paper-font-display1 {
  font-size: 34px;
  font-weight: 400;
  letter-spacing: -.01em;
  line-height: 40px;
}

.paper-font-headline {
  font-size: 24px;
  font-weight: 400;
  letter-spacing: -.012em;
  line-height: 32px;
}

.paper-font-title {
  font-size: 20px;
  font-weight: 500;
  line-height: 28px;
}

.paper-font-subhead {
  font-size: 16px;
  font-weight: 400;
  line-height: 24px;
}

.paper-font-body2 {
  font-size: 14px;
  font-weight: 500;
  line-height: 24px;
}

.paper-font-body1 {
  font-size: 14px;
  font-weight: 400;
  line-height: 20px;
}

.paper-font-caption {
  font-size: 12px;
  font-weight: 400;
  letter-spacing: 0.011em;
  line-height: 20px;
}

.paper-font-menu {
  font-size: 13px;
  font-weight: 500;
  line-height: 24px;
}

.paper-font-button {
  font-size: 14px;
  font-weight: 500;
  letter-spacing: 0.018em;
  line-height: 24px;
  text-transform: uppercase;
}

.paper-font-code2 {
  font-size: 14px;
  font-weight: 700;
  line-height: 20px;
}

.paper-font-code1 {
  font-size: 14px;
  font-weight: 500;
  line-height: 20px;
}

</style>`;
template.setAttribute('style', 'display: none;');
document.head.appendChild(template.content);

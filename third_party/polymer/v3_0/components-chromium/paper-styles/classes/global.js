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
*/

import '../paper-styles-classes.js';

import {html} from '../../polymer/polymer_bundled.min.js';
const template = html`<style>
/* Mixins */

/* [paper-font] */
body {
  font-family: 'Roboto', 'Noto', sans-serif;
  -webkit-font-smoothing: antialiased;  /* OS X subpixel AA bleed bug */
}

/* [paper-font=display2] */
h1 {
  font-size: 45px;
  font-weight: 400;
  letter-spacing: -.018em;
  line-height: 48px;
}

/* [paper-font=display1] */
h2 {
  font-size: 34px;
  font-weight: 400;
  letter-spacing: -.01em;
  line-height: 40px;
}

/* [paper-font=headline] */
h3 {
  font-size: 24px;
  font-weight: 400;
  letter-spacing: -.012em;
  line-height: 32px;
}

/* [paper-font=subhead] */
h4 {
  font-size: 16px;
  font-weight: 400;
  line-height: 24px;
}

/* [paper-font=body2] */
h5, h6 {
  font-size: 14px;
  font-weight: 500;
  line-height: 24px;
}

/* [paper-font=button] */
a {
  font-size: 14px;
  font-weight: 500;
  letter-spacing: 0.018em;
  line-height: 24px;
  text-transform: uppercase;
}

/* Overrides */

body, a {
  color: #212121;
}

h1, h2, h3, h4, h5, h6, p {
  margin: 0 0 20px 0;
}

h1, h2, h3, h4, h5, h6, a {
  text-rendering: optimizeLegibility;
}

a {
  text-decoration: none;
}

</style>`;
template.setAttribute('style', 'display: none;');
document.head.appendChild(template.content);

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

import '../polymer/polymer_bundled.min.js';

import {html} from '../polymer/polymer_bundled.min.js';
const template = html`
<custom-style>
  <style is="custom-style">
    html {

      /* Material Design color palette from online spec document */

      --paper-red-50: #ffebee;
      --paper-red-700: #d32f2f;
      --paper-yellow-500: #ffeb3b;
      --paper-orange-500: #ff9800;

      --paper-grey-50: #fafafa;
      --paper-grey-300: #e0e0e0;
      --paper-grey-400: #bdbdbd;
      --paper-grey-500: #9e9e9e;
      --paper-grey-600: #757575;
      --paper-grey-800: #424242;
      --paper-grey-900: #212121;

      /* opacity for dark text on a light background */
      --dark-secondary-opacity: 0.54;
      --dark-primary-opacity: 0.87;

    }

  </style>
</custom-style>
`;
template.setAttribute('style', 'display: none;');
document.head.appendChild(template.content);

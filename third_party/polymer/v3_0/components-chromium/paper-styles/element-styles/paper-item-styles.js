/**
@license
Copyright (c) 2017 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at
http://polymer.github.io/LICENSE.txt The complete set of authors may be found at
http://polymer.github.io/AUTHORS.txt The complete set of contributors may be
found at http://polymer.github.io/CONTRIBUTORS.txt Code distributed by Google as
part of the polymer project is also subject to an additional IP rights grant
found at http://polymer.github.io/PATENTS.txt
*/
/**
Material design:
[Lists](https://www.google.com/design/spec/components/lists.html)

Shared styles for a native `button` to be used as an item in a `paper-listbox`
element:

    <custom-style>
      <style is="custom-style" include="paper-item-styles"></style>
    </custom-style>

    <paper-listbox>
      <button class="paper-item" role="option">Inbox</button>
      <button class="paper-item" role="option">Starred</button>
      <button class="paper-item" role="option">Sent mail</button>
    </paper-listbox>

@group Paper Elements
@demo demo/index.html
*/

import '../../polymer/polymer_bundled.min.js';
import '../color.js';
import '../default-theme.js';
import '../typography.js';

import {html} from '../../polymer/polymer_bundled.min.js';
const template = html`
<dom-module id="paper-item-styles">
  <template>
    <style>
      html {
        --paper-item: {
          display: block;
          position: relative;
          min-height: var(--paper-item-min-height, 48px);
          padding: 0px 16px;
          @apply --paper-font-subhead;
          border:none;
          outline: none;
          background: white;
          width: 100%;
          text-align: left;
        };
      }
      /* Duplicate the style because of https://github.com/webcomponents/shadycss/issues/193 */
      :host {
        --paper-item: {
          display: block;
          position: relative;
          min-height: var(--paper-item-min-height, 48px);
          padding: 0px 16px;
          @apply --paper-font-subhead;
          border:none;
          outline: none;
          background: white;
          width: 100%;
          text-align: left;
        };
      }

      .paper-item {
        @apply --paper-item;
      }

      .paper-item[hidden] {
        display: none !important;
      }

      .paper-item.iron-selected {
        font-weight: var(--paper-item-selected-weight, bold);
        @apply --paper-item-selected;
      }

      .paper-item[disabled] {
        color: var(--paper-item-disabled-color, var(--disabled-text-color));
        @apply --paper-item-disabled;
      }

      .paper-item:focus {
        position: relative;
        outline: 0;
        @apply --paper-item-focused;
      }

      .paper-item:focus:before {
        position: absolute;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background: currentColor;
        content: '';
        opacity: var(--dark-divider-opacity);
        pointer-events: none;
        @apply --paper-item-focused-before;
      }
    </style>
  </template>
</dom-module>`;
template.setAttribute('style', 'display: none;');
document.head.appendChild(template.content);

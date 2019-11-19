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

/**
The `<iron-flex-layout>` component provides simple ways to use
[CSS flexible box
layout](https://developer.mozilla.org/en-US/docs/Web/Guide/CSS/Flexible_boxes),
also known as flexbox. Note that this is an old element, that was written
before all modern browsers had non-prefixed flex styles. As such, nowadays you
don't really need to use this element anymore, and can use CSS flex styles
directly in your code.

This component provides two different ways to use flexbox:

1. [Layout
classes](https://github.com/PolymerElements/iron-flex-layout/tree/master/iron-flex-layout-classes.html).
The layout class stylesheet provides a simple set of class-based flexbox rules,
that let you specify layout properties directly in markup. You must include this
file in every element that needs to use them.

    Sample use:

    ```
    <custom-element-demo>
      <template>
        <script src="../webcomponentsjs/webcomponents-lite.js"></script>
        <next-code-block></next-code-block>
      </template>
    </custom-element-demo>
    ```

    ```js
    import {html} from '../polymer/polymer_bundled.min.js';
    import '../iron-flex-layout/iron-flex-layout-classes.js';

    const template = html`
      <style is="custom-style" include="iron-flex iron-flex-alignment"></style>
      <style>
        .test { width: 100px; }
      </style>
      <div class="layout horizontal center-center">
        <div class="test">horizontal layout center alignment</div>
      </div>
    `;
    document.body.appendChild(template.content);
    ```

2. [Custom CSS
mixins](https://github.com/PolymerElements/iron-flex-layout/blob/master/iron-flex-layout.html).
The mixin stylesheet includes custom CSS mixins that can be applied inside a CSS
rule using the `@apply` function.

Please note that the old [/deep/ layout
classes](https://github.com/PolymerElements/iron-flex-layout/tree/master/classes)
are deprecated, and should not be used. To continue using layout properties
directly in markup, please switch to using the new `dom-module`-based
[layout
classes](https://github.com/PolymerElements/iron-flex-layout/tree/master/iron-flex-layout-classes.html).
Please note that the new version does not use `/deep/`, and therefore requires
you to import the `dom-modules` in every element that needs to use them.

@group Iron Elements
@pseudoElement iron-flex-layout
@demo demo/index.html
*/
const template = html`
<custom-style>
  <style is="custom-style">
    [hidden] {
      display: none !important;
    }
  </style>
</custom-style>
<custom-style>
  <style is="custom-style">
    html {

      --layout: {
        display: flex;
      };

      --layout-inline: {
        display: inline-flex;
      };

      --layout-horizontal: {
        @apply --layout;

        flex-direction: row;
      };

      --layout-horizontal-reverse: {
        @apply --layout;

        flex-direction: row-reverse;
      };

      --layout-vertical: {
        @apply --layout;

        flex-direction: column;
      };

      --layout-vertical-reverse: {
        @apply --layout;

        flex-direction: column-reverse;
      };

      --layout-wrap: {
        flex-wrap: wrap;
      };

      --layout-wrap-reverse: {
        flex-wrap: wrap-reverse;
      };

      --layout-flex-auto: {
        flex: 1 1 auto;
      };

      --layout-flex-none: {
        flex: none;
      };

      --layout-flex: {
        flex: 1;
        flex-basis: 0.000000001px;
      };

      --layout-flex-2: {
        flex: 2;
      };

      --layout-flex-3: {
        flex: 3;
      };

      --layout-flex-4: {
        flex: 4;
      };

      --layout-flex-5: {
        flex: 5;
      };

      --layout-flex-6: {
        flex: 6;
      };

      --layout-flex-7: {
        flex: 7;
      };

      --layout-flex-8: {
        flex: 8;
      };

      --layout-flex-9: {
        flex: 9;
      };

      --layout-flex-10: {
        flex: 10;
      };

      --layout-flex-11: {
        flex: 11;
      };

      --layout-flex-12: {
        flex: 12;
      };

      /* alignment in cross axis */

      --layout-start: {
        align-items: flex-start;
      };

      --layout-center: {
        align-items: center;
      };

      --layout-end: {
        align-items: flex-end;
      };

      --layout-baseline: {
        align-items: baseline;
      };

      /* alignment in main axis */

      --layout-start-justified: {
        justify-content: flex-start;
      };

      --layout-center-justified: {
        justify-content: center;
      };

      --layout-end-justified: {
        justify-content: flex-end;
      };

      --layout-around-justified: {
        justify-content: space-around;
      };

      --layout-justified: {
        justify-content: space-between;
      };

      --layout-center-center: {
        @apply --layout-center;
        @apply --layout-center-justified;
      };

      /* self alignment */

      --layout-self-start: {
        align-self: flex-start;
      };

      --layout-self-center: {
        align-self: center;
      };

      --layout-self-end: {
        align-self: flex-end;
      };

      --layout-self-stretch: {
        align-self: stretch;
      };

      --layout-self-baseline: {
        align-self: baseline;
      };

      /* multi-line alignment in main axis */

      --layout-start-aligned: {
        align-content: flex-start;
      };

      --layout-end-aligned: {
        align-content: flex-end;
      };

      --layout-center-aligned: {
        align-content: center;
      };

      --layout-between-aligned: {
        align-content: space-between;
      };

      --layout-around-aligned: {
        align-content: space-around;
      };

      /*******************************
                Other Layout
      *******************************/

      --layout-block: {
        display: block;
      };

      --layout-invisible: {
        visibility: hidden !important;
      };

      --layout-relative: {
        position: relative;
      };

      --layout-fit: {
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        left: 0;
      };

      --layout-scroll: {
        -webkit-overflow-scrolling: touch;
        overflow: auto;
      };

      --layout-fullbleed: {
        margin: 0;
        height: 100vh;
      };

      /* fixed position */

      --layout-fixed-top: {
        position: fixed;
        top: 0;
        left: 0;
        right: 0;
      };

      --layout-fixed-right: {
        position: fixed;
        top: 0;
        right: 0;
        bottom: 0;
      };

      --layout-fixed-bottom: {
        position: fixed;
        right: 0;
        bottom: 0;
        left: 0;
      };

      --layout-fixed-left: {
        position: fixed;
        top: 0;
        bottom: 0;
        left: 0;
      };

    }
  </style>
</custom-style>`;

template.setAttribute('style', 'display: none;');
document.head.appendChild(template.content);

var style = document.createElement('style');
style.textContent = '[hidden] { display: none !important; }';
document.head.appendChild(style);
